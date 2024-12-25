#include "tree.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

Tree::~Tree() {
    delete_node(root);
}

void Tree::push(int id) {
    root = push(root, id);
}

void Tree::kill(int id) {
    root = kill(root, id);
}

std::vector<int> Tree::get_nodes() {
    std::vector<int> result;
    get_nodes(root, result);
    return result;
}

void Tree::delete_node(Node* node) {
    if (node == NULL) {
        return;
    }
    for(auto child : node->children){
        delete_node(child);
    }
    node->children.clear();
    delete node;
}

void Tree::get_nodes(Node* node, std::vector<int>& v) {
    if (node == NULL) {
        return;
    }
    v.push_back(node->id);
    for(auto child : node->children){
        get_nodes(child, v);
    }
}

Node* Tree::push(Node* root, int val) {
    if (root == NULL) {
        root = new Node;
        root->id = val;
        root->found = false;
        return root;
    } else {
        root->children.push_back(push(NULL, val));
    }
    return root;
}

Node* Tree::kill(Node* root_node, int val) {
    if (root_node == NULL) {
        return NULL;
    }

    for(auto it = root_node->children.begin(); it != root_node->children.end(); ){
        if((*it)->id == val){
            delete_node(*it);
            it = root_node->children.erase(it);
        }
        else{
            *it = kill(*it, val);
            ++it;
        }
    }
    return root_node;
}
