#pragma once
#include <vector>

struct Node {
    int id;
    Node* left;
    Node* right;
    bool found;
};

class Tree {
public:
    void push(int);
    void kill(int);
    std::vector<int> get_nodes();
    ~Tree();
    Node* root = NULL;

private:
    Node* push(Node* t, int);
    Node* kill(Node* t, int);
    void get_nodes(Node*, std::vector<int>&);
    void delete_node(Node*);
};

void print_tree(Node* root, int depth);