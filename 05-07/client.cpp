#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <exception>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <zmq.hpp>

using namespace std;

const int TIMER = 500;
const int DEFAULT_PORT = 5050;
int n = 2;
std::map<std::string, int> m;

struct Child {
    int id;
    int pid;
    zmq::socket_t* socket;
};

bool send_message(zmq::socket_t& socket, const string& message_string) {
    zmq::message_t message(message_string.size());
    memcpy(message.data(), message_string.c_str(), message_string.size());
    auto result = socket.send(message, zmq::send_flags::none);
    return result.has_value();
}

string receive_message(zmq::socket_t& socket) {
    zmq::message_t message;
    bool ok = false;
    try {
        auto result = socket.recv(message, zmq::recv_flags::none);
        ok = result.has_value();
    } catch (...) {
        ok = false;
    }
    string received_message(static_cast<char*>(message.data()), message.size());
    if (received_message.empty() || !ok) {
        return "";
    }
    return received_message;
}

void create_node(int id, int port) {
    char* arg0 = strdup("./client");
    char* arg1 = strdup((to_string(id)).c_str());
    char* arg2 = strdup((to_string(port)).c_str());
    char* args[] = {arg0, arg1, arg2, NULL};
    execv("./client", args);
}

string get_port_name(const int port) {
    return "tcp://127.0.0.1:" + to_string(port);
}

void real_create(zmq::socket_t& parent_socket, zmq::context_t& context, vector<Child>& children, int create_id, int& id, int& pid) {
    cout << to_string(id);
    if (pid == -1) {
        send_message(parent_socket, "Error: Cannot fork");
        pid = 0;
    } else if (pid == 0) {
        create_node(create_id, DEFAULT_PORT + create_id);
    } else {
        zmq::socket_t* child_socket = new zmq::socket_t(context, ZMQ_REQ);
        child_socket->connect(get_port_name(DEFAULT_PORT + create_id));
        child_socket->set(zmq::sockopt::rcvtimeo, TIMER);
        child_socket->set(zmq::sockopt::sndtimeo, TIMER);
        Child child;
        child.id = create_id;
        child.pid = pid;
        child.socket = child_socket;
        children.push_back(child);
        id = create_id;
        send_message(*child_socket, "pid");
        send_message(parent_socket, receive_message(*child_socket));
    }
}

void real_kill(zmq::socket_t& parent_socket, vector<Child>& children, int delete_id, int& id, string& request_string) {
    if (id == 0) {
        send_message(parent_socket, "Error: Not found");
    } else if (id == delete_id) {
        for(auto& child : children){
            send_message(*child.socket, "kill_children");
            receive_message(*child.socket);
            kill(child.pid, SIGTERM);
            kill(child.pid, SIGKILL);
            delete child.socket;
        }
        children.clear();
        id = 0;
        send_message(parent_socket, "Ok");
    } else {
        for(auto& child : children){
            send_message(*child.socket, request_string);
            string response = receive_message(*child.socket);
            if(!response.empty()){
                send_message(parent_socket, response);
            }
        }
    }
}

void real_exec(zmq::socket_t& parent_socket, vector<Child>& children, int id, const string& request_string) {
    if (id == 0) {
        string receive_message = "Error:" + to_string(id) + ": Not found";
        send_message(parent_socket, receive_message);
    } else {
        for(auto& child : children){
            send_message(*child.socket, request_string);
            string str = receive_message(*child.socket);
            if (str.empty()) str = "Error: Node is unavailable";
            send_message(parent_socket, str);
        }
    }
}

void real_ping(zmq::socket_t& parent_socket, vector<Child>& children, int id, const string& request_string) {
    if (id == 0) {
        string receive_message = "Error:" + to_string(id) + ": Not found";
        send_message(parent_socket, receive_message);
    } else {
        for(auto& child : children){
            send_message(*child.socket, request_string);
            string str = receive_message(*child.socket);
            if (str.empty()) str = "Ok: 0";
            send_message(parent_socket, str);
        }
    }
}

void real_heartbeat(zmq::socket_t& parent_socket, vector<Child>& children, int id, const string& request_string) {
    if (id == 0) {
        string receive_message = "Error:" + to_string(id) + ": Not found";
        send_message(parent_socket, receive_message);
    } else {
        for(auto& child : children){
            send_message(*child.socket, request_string);
            string str = receive_message(*child.socket);
            if (str.empty()) str = "Ok: 0";
            send_message(parent_socket, str);
        }
    }
}

void exec_command(istringstream& command_stream, zmq::socket_t& parent_socket, zmq::context_t& context, vector<Child>& children, int& id, string& request_string) {
    string name, value;
    int exec_id;
    command_stream >> exec_id;
    if (exec_id == id) {
        command_stream >> name;
        command_stream >> value;
        string receive_message = "";
        string answer = "";

        if (value == "NOVALUE") {
            receive_message = "Ok:" + to_string(id) + ":";
            if (m.find(name) != m.end()) {
                receive_message += to_string(m[name]);
            } else {
                receive_message += " '" + name + "' not found";
            }
        } else {
            m[name] = stoi(value);
            receive_message = "Ok:" + to_string(id);
        }
        send_message(parent_socket, receive_message);
    } else {
        real_exec(parent_socket, children, id, request_string);
    }
}

void ping_command(istringstream& command_stream, zmq::socket_t& parent_socket, vector<Child>& children, int& id, string& request_string) {
    int ping_id;
    string receive_message;
    command_stream >> ping_id;
    if (ping_id == id) {
        receive_message = "Ok: 1";
        send_message(parent_socket, receive_message);
    } else {
        real_ping(parent_socket, children, id, request_string);
    }
}

void heartbeat_command(istringstream& command_stream, zmq::socket_t& parent_socket, vector<Child>& children, int& id, string& request_string) {
    int ping_id;
    int ping_time;
    string receive_message;
    command_stream >> ping_id;
    command_stream >> ping_time;
    if (ping_id == id) {
        receive_message = "Available:" + to_string(id);
        send_message(parent_socket, receive_message);
    } else {
        real_heartbeat(parent_socket, children, id, request_string);
    }
}

void kill_children(zmq::socket_t& parent_socket, vector<Child>& children) {
    for(auto& child : children){
        send_message(*child.socket, "kill_children");
        receive_message(*child.socket);
        kill(child.pid, SIGTERM);
        kill(child.pid, SIGKILL);
        delete child.socket;
    }
    children.clear();
    send_message(parent_socket, "Ok");
}

int main(int argc, char** argv) {
    if(argc < 3){
        cerr << "Usage: ./client <id> <parent_port>\n";
        return 1;
    }
    int id = stoi(argv[1]);
    int parent_port = stoi(argv[2]);
    zmq::context_t context(3);
    zmq::socket_t parent_socket(context, ZMQ_REP);
    parent_socket.connect(get_port_name(parent_port));
    parent_socket.set(zmq::sockopt::rcvtimeo, TIMER);
    parent_socket.set(zmq::sockopt::sndtimeo, TIMER);
    vector<Child> children;
    string request_string;

    while (true) {
        request_string = receive_message(parent_socket);
        if(request_string.empty()){
            continue;
        }
        istringstream command_stream(request_string);
        string command;
        command_stream >> command;
        if (command == "id") {
            string parent_string = "Ok:" + to_string(id);
            send_message(parent_socket, parent_string);
        } else if (command == "pid") {
            string parent_string = "Ok:" + to_string(getpid());
            send_message(parent_socket, parent_string);
        } else if (command == "create") {
            int create_id;
            command_stream >> create_id;
            if (create_id == id) {
                string message_string = "Error: Already exists";
                send_message(parent_socket, message_string);
            } else {
                bool exists = false;
                for(auto& child : children){
                    if(child.id == create_id){
                        exists = true;
                        break;
                    }
                }
                if(exists){
                    send_message(parent_socket, "Error: Child already exists");
                }
                else{
                    pid_t pid = fork();
                    if(pid < 0){
                        send_message(parent_socket, "Error: Cannot fork");
                    }
                    else if(pid == 0){
                        create_node(create_id, DEFAULT_PORT + create_id);
                    }
                    else{
                        zmq::socket_t* child_socket = new zmq::socket_t(context, ZMQ_REQ);
                        child_socket->connect(get_port_name(DEFAULT_PORT + create_id));
                        child_socket->set(zmq::sockopt::rcvtimeo, TIMER);
                        child_socket->set(zmq::sockopt::sndtimeo, TIMER);
                        Child child;
                        child.id = create_id;
                        child.pid = pid;
                        child.socket = child_socket;
                        children.push_back(child);
                        send_message(parent_socket, "Ok");
                    }
                }
            }
        } else if (command == "kill") {
            int delete_id;
            command_stream >> delete_id;
            if (delete_id == id) {
                kill_children(parent_socket, children);
                break;
            }
            else{
                bool found = false;
                for(auto it = children.begin(); it != children.end(); ++it){
                    if(it->id == delete_id){
                        send_message(*(it->socket), "kill_children");
                        string received_message = receive_message(*(it->socket));
                        kill(it->pid, SIGTERM);
                        kill(it->pid, SIGKILL);
                        delete it->socket;
                        children.erase(it);
                        send_message(parent_socket, "Ok");
                        found = true;
                        break;
                    }
                }
                if(!found){
                    send_message(parent_socket, "Error: Not found");
                }
            }
        } else if (command == "exec") {
            exec_command(command_stream, parent_socket, context, children, id, request_string);
        } else if (command == "ping") {
            ping_command(command_stream, parent_socket, children, id, request_string);
        } else if (command == "heartbeat") {
            heartbeat_command(command_stream, parent_socket, children, id, request_string);
        } else if (command == "kill_children") {
            kill_children(parent_socket, children);
            break;
        }
        if (parent_port == 0) {
            break;
        }
    }
    for(auto& child : children){
        delete child.socket;
    }
    return 0;
}
