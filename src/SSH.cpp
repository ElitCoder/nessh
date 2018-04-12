#include "SSH.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

#include <sys/stat.h>
#include <libssh/sftp.h>
#include <fcntl.h>

#define ERROR(...)	do { fprintf(stderr, "Error: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1); } while(0)

using namespace std;

SSH::SSH(const string& ip, const string& pass) :
	ip_(ip), pass_(pass) {
	user_ = "";
	
	connected_ = false;
}

SSH::SSH(const string& ip, const string& user, const string& pass) {
	ip_ = ip;
	user_ = user;
	pass_ = pass;
	
	connected_ = false;
}

void SSH::clearOutput() {
	output_.clear();
}

vector<string> SSH::getOutput() {
	return output_;
}

void SSH::disconnect() {
	if (!connected_)
		return;
		
	ssh_disconnect(session_);
	ssh_free(session_);
}

static vector<string> splitString(const string& input, char split) {
	vector<string> splitted;
	size_t last_split = 0;
	
	for (size_t i = 0; i < input.length(); i++) {
		if (input.at(i) == split) {
			splitted.push_back(input.substr(last_split, i - last_split));
			last_split = i + 1;
		}
	}
	
	// Don't forget last token
	if (last_split != input.length()) {
		splitted.push_back(input.substr(last_split, input.length() - last_split));
	}
	
	return splitted;
}

static string getFilenameFromPath(const string& path) {
	for (int i = path.length() - 1; i >= 0; i--)
		if (path.at(i) == '/')
			return path.substr(i + 1);
			
	return "";		
}

static size_t getFileSize(string& filename) {
	ifstream file(filename, ios::binary | ios::ate);
	
	if (!file.is_open()) {
		cout << "Warning: could not open file to get file size\n";
		
		return 0;
	}
	
	size_t size = file.tellg();
	file.close();
	
	return size;
}

bool SSH::fileExists(const string& path, const string& filename) {
	sftp_session sftp = sftp_new(session_);
	
	if (sftp == NULL) {
		cout << "ERROR: Allocating SFTP session: " << ssh_get_error(session_) << endl;
		
		return false;
	}
	
	if (sftp_init(sftp) != SSH_OK) {
		cout << "ERROR: Initializing SFTP session: " << sftp_get_error(sftp) << endl;
		
		sftp_free(sftp);
		return false;
	}
	
	sftp_file file = sftp_open(sftp, (path + filename).c_str(), O_RDONLY, 0);
	bool result = false;
	
	if (file != NULL) {
		sftp_close(file);
		result = true;
	}
	
	sftp_free(sftp);
	return result;
}

bool SSH::transferRemote(const string& from, const string& to, bool overwrite) {
	if (!connected_) {
		cout << "Warning: can't read from SCP without an active SSH connection\n";
		
		return false;
	}
	
	ssh_scp scp = ssh_scp_new(session_, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, to.c_str());
	
	if (scp == NULL) {
		cout << "Error: could not create SCP\n";
		
		return false;
	}
	
	if (ssh_scp_init(scp) != SSH_OK) {
		cout << "Error: could not initialize SCP session\n";
		
		ssh_scp_free(scp);
		return false;
	}

	vector<string> files = splitString(from, ' ');
	
	for (auto& filename : files) {
		string remote_file = getFilenameFromPath(filename);
		
		// Check if the file already exists
		if (!overwrite) {
			if (fileExists(to, remote_file))
				continue;
		}
		
		ifstream file(filename);
		
		if (!file.is_open()) {
			cout << "Warning: could not write file to remote host (" << filename << ")\n";
			
			ssh_scp_close(scp);
			ssh_scp_free(scp);
			return false;
		}
		
		const size_t FILE_BUFFER_SIZE = 16384;
		char* file_buffer = new char[FILE_BUFFER_SIZE];
		
		size_t file_size = getFileSize(filename);

		if (ssh_scp_push_file(scp, remote_file.c_str(), file_size, S_IRWXU) != SSH_OK) {
			cout << "Warning: could not push file to remote host with RWX\n";
			
			ssh_scp_close(scp);
			ssh_scp_free(scp);
			delete[] file_buffer;
			return false;
		}
		
		size_t left = file_size;
		
		while (true) {
			size_t read_amount = left > FILE_BUFFER_SIZE ? FILE_BUFFER_SIZE : left;
			file.read(file_buffer, read_amount);
			
			if (!file)
				ERROR("error reading file\n");
				
			int wrote = ssh_scp_write(scp, file_buffer, read_amount);
			
			if (wrote != SSH_OK) {
				cout << "Warning: could not write data to remote file\n";
				
				ssh_scp_close(scp);
				ssh_scp_free(scp);
				delete[] file_buffer;
				return false;
			}
			
			left -= read_amount;
			
			if (left == 0)
				break;
		}
		
		file.close();
		
		delete[] file_buffer;
		
		//cout << "Wrote local file " << filename << " to remote file " << remote_file << endl;
	}
	
	ssh_scp_close(scp);
	ssh_scp_free(scp);
	
	return true;
}

bool SSH::transferLocal(const string& from, const string& to, const string& custom_filename) {
	if (!connected_) {
		cout << "Warning: can't read from SCP without an active SSH connection\n";
		
		return false;
	}
		
	ssh_scp scp = ssh_scp_new(session_, SSH_SCP_READ | SSH_SCP_RECURSIVE, from.c_str());
	
	if (scp == NULL) {
		cout << "Error: could not create SCP\n";
		
		return false;
	}
	
	if (ssh_scp_init(scp) != SSH_OK) {
		cout << "Error: could not create reading SCP session\n";
		
		ssh_scp_free(scp);
		return false;
	}
	
	bool succeeded = true;
	
	do {
		bool end = false;
		
		switch (ssh_scp_pull_request(scp)) {
			case SSH_SCP_REQUEST_NEWFILE: {
				size_t size = ssh_scp_request_get_size(scp);
				string filename = ssh_scp_request_get_filename(scp);
				string actual_filename = custom_filename == "" ? (to + "/" + filename) : custom_filename; 
				const size_t FILE_BUFFER_SIZE = 16384;
				char* file_buffer = new char[FILE_BUFFER_SIZE];
				
				//cout << "Downloading file: " << filename << " with size " << size << endl;
				
				//ofstream file(to + "/" + filename);
				ofstream file(actual_filename);
				
				if (!file.is_open()) {
					cout << "Warning: could not open file for writing local SCP\n";
					
					delete[] file_buffer;
					succeeded = false;
					end = true;
					break;
				}
				
				size_t left = size;
				ssh_scp_accept_request(scp);
				
				while (true) {
					int read = ssh_scp_read(scp, file_buffer, left > FILE_BUFFER_SIZE ? FILE_BUFFER_SIZE : left);
					
					if (read == SSH_ERROR) {
						cout << "Error reading SCP\n";
					
						succeeded = false;
						end = true;
						break;
					}
					
					file.write(file_buffer, read);
					
					left -= read;
					
					if (left == 0)
						break;
				}
				
				file.close();
				delete[] file_buffer;
				
				break;
			}
			
			case SSH_ERROR: {
				cout << "Error: SCP encountered an error (" << ssh_get_error(session_) << ")\n";
				
				succeeded = false;
				end = true;
				break;
			}
			
			case SSH_SCP_REQUEST_WARNING: {
				cout << "Warning: received a warning from SCP (" << ssh_scp_request_get_warning(scp) << ")\n";
				
				break;
			}
			
			case SSH_SCP_REQUEST_EOF: {
				end = true;
				break;
			}
		}
		
		if (end)
			break;
	} while (true);
	
	ssh_scp_close(scp);
    ssh_scp_free(scp);
	
	return succeeded;
}

bool SSH::connect() {
	session_ = ssh_new();
	
	if (session_ == NULL) {
		cout << "Error: could not create SSH session\n";
		
		ssh_free(session_);
		return false;
	}
	
	string real_user = user_.length() == 0 ? "root" : user_;
	
	ssh_options_set(session_, SSH_OPTIONS_HOST, ip_.c_str());
	ssh_options_set(session_, SSH_OPTIONS_USER, real_user.c_str());
	ssh_options_set(session_, SSH_OPTIONS_STRICTHOSTKEYCHECK, 0 /* Do not ask for fingerprint approval */);
	
	if (ssh_connect(session_) != SSH_OK) {
		cout << "Error: could not connect to " << ip_ << " code: " << ssh_get_error(session_) << endl;
		
		ssh_free(session_);
		return false;
	}
	
	if (ssh_userauth_password(session_, NULL, pass_.c_str()) != SSH_AUTH_SUCCESS) {
		cout << "Error: wrong password for " << ip_ << endl;
		
		disconnect();
		return false;
	}
	
	connected_ = true;
	
	//cout << "Debug: connected to " << ip_ << endl;
	
	return true;
}

static string getTimestamp() {
	time_t current_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	
	return ctime(&current_time);
}

bool SSH::command(const string& command, bool output_file, bool output_vector) {
	if (!connected_) {
		cout << "Error: could not execute command, we're not connected\n";
		
		return false;
	}
	
	ssh_channel channel = ssh_channel_new(session_);
	
	if (channel == NULL) {
		cout << "Error: could not create channel\n";
		
		return false;
	}
	
	if (ssh_channel_open_session(channel) != SSH_OK) {
		cout << "Error: could not open channel\n";
		
		ssh_channel_free(channel);
		return false;
	}
	
	if (ssh_channel_request_exec(channel, command.c_str()) != SSH_OK) {
		cout << "Error: could not execute command\n";
		
		ssh_channel_close(channel);
		ssh_channel_free(channel);
		return false;
	}
	
	if (output_file || output_vector) {
		ofstream* file = nullptr;
		
		if (output_file) {
			string filename = "stdout_" + ip_;
			file = new ofstream(filename, ios_base::app);
			
			if (!file->is_open()) {
				delete file;
				
				goto end;
			}
			
			//cout << "Debug: writing to log\n";
			
			string current_time = getTimestamp();
			
			file->write("[", 1);
			file->write(current_time.c_str(), current_time.length() - 1);
			file->write("]\n", 2);
		} else {
			//cout << "Debug: writing to vector\n";
			output_.clear();
		}

		char buffer[256];
		
		for (size_t i = 0; i <= 1; i++) {
			do {
				int bytes_received = ssh_channel_read(channel, buffer, sizeof(buffer), i);
				
				if (bytes_received <= 0)
					break;
					
				//cout << "Debug: read " << bytes_received << " bytes from remote\n";
				
				if (output_file)
					file->write(buffer, bytes_received);
				else
					output_.push_back(string(buffer, bytes_received));
			} while (true);
		}
		
		if (output_file) {
			file->close();
			delete file;
		}
	}
	
end:
	ssh_channel_send_eof(channel);
	ssh_channel_close(channel);
	ssh_channel_free(channel);
	
	return true;
}

bool SSH::operator==(const string& ip) {
	return ip == ip_;
}