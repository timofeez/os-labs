#include <signal.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <zmq.hpp>

#include "tree.hpp"

using namespace std;

const int TIMER = 500;
const int DEFAULT_PORT = 5050;
int n = 2, flag_exit = 1;

pthread_mutex_t mutex1;
zmq::context_t context(1);
zmq::socket_t main_socket(context, ZMQ_REQ);

struct ChildNode {
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
        return "Root is dead";
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

bool is_number(string val) {
    try {
        int tmp = stoi(val);
        return true;
    } catch (exception& e) {
        cout << "Error: " << e.what() << "\n";
        return false;
    }
}

typedef struct {
    int ping_time;
    int ping_id;
} heartbeat_params;


void* heartbeat_iter(void* param) {
    heartbeat_params* heartbeat_param = (heartbeat_params*)param;
    chrono::milliseconds timespan(heartbeat_param->ping_time);
    string message_string = "heartbeat " + to_string(heartbeat_param->ping_id) + " " + to_string(heartbeat_param->ping_time);
    int count = 0;
    for (int j = 0; j < 4; j++) {
        pthread_mutex_lock(&mutex1);
        
        
        send_message(main_socket, message_string);
        string received_message = receive_message(main_socket);
        pthread_mutex_unlock(&mutex1);
        this_thread::sleep_for(timespan);
        if (received_message.substr(0, min<int>(received_message.size(), 9)) != "Available") {
            break;
        }
        count += 1;
    }
    if (count == 0) {
        cout << "Node " + to_string(heartbeat_param->ping_id) + " is unavailable.\n";
    } else {
        cout << "Node " + to_string(heartbeat_param->ping_id) + " is available.\n";
    }

    pthread_exit(0);
}

int main() {
    Tree T;
    std::vector<int> nodes;
    string command;
    int child_pid = 0;
    int child_id = 0;
    pthread_mutex_init(&mutex1, NULL);
    cout << "Commands:\n";
    cout << "1. create (id)\n";
    cout << "2. exec (id) (name, value)\n";
    cout << "3. ping (id)\n";
    cout << "4. heartbeat (ping_time)\n";
    cout << "5. exit\n" << endl;
    
    vector<ChildNode> child_nodes;
    while (true) {
        cin >> command;
        if (command == "create") {
            n++;
            size_t node_id = 0;
            string str = "";
            string result = "";
            cin >> str;
            if (!is_number(str)) {
                continue;
            }
            node_id = stoi(str);
            if (child_pid == 0) {
                main_socket.bind(get_port_name(DEFAULT_PORT + node_id));
                main_socket.set(zmq::sockopt::rcvtimeo, n * TIMER);
                main_socket.set(zmq::sockopt::sndtimeo, n * TIMER);
                child_pid = fork();
                if (child_pid == -1) {
                    cerr << "Unable to create first worker node\n";
                    child_pid = 0;
                    exit(1);
                } else if (child_pid == 0) {
                    create_node(node_id, DEFAULT_PORT + node_id);
                } else {
                    child_id = node_id;
                    main_socket.set(zmq::sockopt::rcvtimeo, n * TIMER);
                    main_socket.set(zmq::sockopt::sndtimeo, n * TIMER);
                    send_message(main_socket, "pid");
                    result = receive_message(main_socket);
                    
                    zmq::socket_t* child_socket = new zmq::socket_t(context, ZMQ_REQ);
                    child_socket->connect(get_port_name(DEFAULT_PORT + node_id));
                    child_socket->set(zmq::sockopt::rcvtimeo, n * TIMER);
                    child_socket->set(zmq::sockopt::sndtimeo, n * TIMER);
                    ChildNode cn;
                    cn.id = node_id;
                    cn.pid = child_pid; 
                    cn.socket = child_socket;
                    child_nodes.push_back(cn);
                }
            } else {
                string msg_s = "create " + to_string(node_id);
                send_message(main_socket, msg_s);
                result = receive_message(main_socket);
                if (result.substr(0, 2) == "Ok") {
                    T.push(node_id);
                    nodes.push_back(node_id);
                }
            }
            cout << result << "\n";
        } else if (command == "kill") {
            int node_id = 0;
            string str = "";
            cin >> str;
            if (!is_number(str)) {
                continue;
            }
            node_id = stoi(str);
            if (child_pid == 0) {
                cout << "Error: Not found\n";
                continue;
            }
            if (node_id == child_id) {
                kill(child_pid, SIGTERM);
                kill(child_pid, SIGKILL);
                child_id = 0;
                child_pid = 0;
                T.kill(node_id);
                cout << "Ok\n";
                continue;
            }
            
            bool found = false;
            for(auto it = child_nodes.begin(); it != child_nodes.end(); ++it){
                if(it->id == node_id){
                    send_message(*(it->socket), "kill_children");
                    string received_message = receive_message(*(it->socket));
                    kill(it->pid, SIGTERM);
                    kill(it->pid, SIGKILL);
                    delete it->socket;
                    child_nodes.erase(it);
                    T.kill(node_id);
                    cout << received_message << "\n";
                    found = true;
                    break;
                }
            }
            if(!found){
                cout << "Error: Not found\n";
            }
        } else if (command == "exec") {
            string input_string;
            string id_str = "";
            string name = "";
            string value = "0";
            int id = 0;
            getline(cin, input_string);
            istringstream iss(input_string);
            vector<std::string> words;
            std::string word;
            while (iss >> word) {
                words.push_back(word);
            }
            if(words.empty()){
                continue;
            }
            id_str = words[0];
            if (!is_number(id_str)) {
                continue;
            }
            id = stoi(id_str);
            if(words.size() >=2){
                name = words[1];
            }
            if (words.size() == 2) {
                string message_string = "exec " + to_string(id) + " " + name + " " + "NOVALUE";
                send_message(main_socket, message_string);
                string received_message = receive_message(main_socket);
                cout << received_message << "\n";
            }

            if (words.size() == 3) {
                value = words[2];
                string message_string = "exec " + to_string(id) + " " + name + " " + value;
                send_message(main_socket, message_string);
                string received_message = receive_message(main_socket);
                cout << received_message << "\n";
            }

        } else if (command == "ping") {
            string id_str = "";
            int id = 0;
            cin >> id_str;
            if (!is_number(id_str)) {
                continue;
            }
            id = stoi(id_str);
            string message_string = "ping " + to_string(id);
            send_message(main_socket, message_string);
            string received_message = receive_message(main_socket);
            cout << received_message << "\n";
        } else if (command == "heartbeat") {
            string time_str = "";
            int ping_time = 0;
            cin >> time_str;
            if (!is_number(time_str)) {
                continue;
            }
            ping_time = stoi(time_str);
            
            std::vector<int> check_nodes = T.get_nodes();
            if (check_nodes.empty()) {
                cout << "There isn't calculation nodes" << endl;
                continue;
            }
            vector<pthread_t> tid(check_nodes.size());
            vector<heartbeat_params> hb(check_nodes.size());
            for (int i = 0; i < check_nodes.size(); i++) {
                hb[i].ping_time = ping_time;
                hb[i].ping_id = check_nodes[i];
                pthread_create(&tid[i], NULL, heartbeat_iter, &hb[i]);
            }

            for (int i = 0; i < check_nodes.size(); i++) {
                pthread_join(tid[i], NULL);
            }
        } else if (command == "exit") {
            try {
                
                for(auto& cn : child_nodes){
                    kill(cn.pid, SIGTERM);
                    kill(cn.pid, SIGKILL);
                    delete cn.socket;
                }
                system("killall client");
                flag_exit = 0;
            } catch (exception& e) {
                cout << "Error: " << e.what() << "\n";
            }

            break;
        }
    }
    return 0;
}
