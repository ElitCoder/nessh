#ifndef SSH_H
#define SSH_H

#include <string>
#include <vector>

// Fuck the C++ wrapper
#include <libssh/libssh.h>

class SSH {
public:
	SSH(const std::string& ip, const std::string& pass);
	SSH(const std::string& ip, const std::string& user, const std::string& pass);
	
	bool connect();
	void disconnect();
	bool command(const std::string& command, bool output_file = false, bool output_vector = false);
	bool transferLocal(const std::string& from, const std::string& to, const std::string& custom_filename);
	bool transferRemote(const std::string& from, const std::string& to);
	
	void clearOutput();
	std::vector<std::string> getOutput();
	
	bool operator==(const std::string& ip);
	
private:
	std::string ip_;
	std::string user_;
	std::string pass_;
	
	bool connected_;
	ssh_session session_;
	
	std::vector<std::string> output_;
};

#endif