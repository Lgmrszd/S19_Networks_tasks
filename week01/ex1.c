#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
// Stack errors
#define CODE_STACK_OK 0
#define CODE_STACK_EMPTY -1
// Client-Server codes
#define QUERY 100
#define Q_PEEK 105
#define Q_PUSH 110
#define Q_POP 115
#define Q_EMPTY 120
#define Q_DISPLAY 125
#define Q_CREATE 130
#define Q_STACKSIZE 135
#define OK 200
#define ERROR 400
#define E_STACK_NULL 405
#define E_STACK_EMPTY 410
#define EXIT 300
// Other constants
#define READ_SIZE 32

// Debug flag
//#define DEBUG

// Stack implementation
int stack_err = 0;
typedef struct LinkedNode {
    int data;
    struct LinkedNode* prev;
} LinkedNode;

typedef struct LinkedStack {
    struct LinkedNode* last;
} LinkedStack;

LinkedStack* stack_create() {
    LinkedStack* stack = (LinkedStack*) malloc(sizeof(LinkedStack));
    stack->last = NULL;
    stack_err = CODE_STACK_OK;
    return stack;
}

int stack_peek(LinkedStack* stack) {
    if (stack->last == NULL) {
        stack_err = CODE_STACK_EMPTY;
        return 0;
    } else {
        stack_err = CODE_STACK_OK;
        return stack->last->data;
    }
}

void stack_push(LinkedStack* stack, int data) {
    LinkedNode* node = stack->last;
    LinkedNode* new_node = (LinkedNode*) malloc(sizeof(LinkedNode));
    new_node->data = data;
    if (node == NULL) {
        new_node->prev = NULL;
    } else {
        new_node->prev = node;
    }
    stack->last = new_node;
    stack_err = CODE_STACK_OK;
}

void stack_pop(LinkedStack* stack) {
    if (stack->last == NULL) {
        stack_err = CODE_STACK_EMPTY;
    } else {
        LinkedNode* prev_node = stack->last->prev;
        free(stack->last);
        stack->last = prev_node;
        stack_err = CODE_STACK_OK;
    }
}

int stack_empty(LinkedStack* stack) {
    stack_err = CODE_STACK_OK;
    return !stack->last;
}

void destroy_stack(LinkedStack* stack) {
    if (!stack)
        return;
    LinkedNode* node = stack->last;
    LinkedNode* prev_node;
    free(stack);
    while(node) {
        prev_node = node->prev;
        free(node);
        node = prev_node;
    }
}

int stack_size(LinkedStack* stack) {
    int size = 0;
    LinkedNode* node = stack->last;
    while (node) {
        node = node->prev;
        size++;
    }
    stack_err = CODE_STACK_OK;
    return size;
}
// Server functions implementation
LinkedStack* server_stack;

void create() {
    destroy_stack(server_stack);
    server_stack = stack_create();
}

int peek() {
    return stack_peek(server_stack);
}

void push(int data) {
    stack_push(server_stack, data);
}

void pop() {
    stack_pop(server_stack);
}

int empty() {
    return stack_empty(server_stack);
}

int server_stack_size() {
    return stack_size(server_stack);
}

void display() {
//    int width = 15;
    printf("SERVER: display:   |              |\n");
    printf("SERVER: display:   |--------------|\n");
    LinkedNode* node = server_stack->last;
    while (node) {
        printf("SERVER: display:   |%14d|\n", node->data);
        printf("SERVER: display:   |--------------|\n");
        node = node->prev;
    }
    printf("SERVER: display:   |====bottom====|\n");
}

// Server implementation
typedef struct packet {
    int code;
    int data;
} packet;

size_t packet_size = sizeof(packet);
packet* in_packet;
packet* out_packet;

void form_packet(packet* new_packet, int code, int data) {
    new_packet->code = code;
    new_packet->data = data;
}

void server_routine(int fd_in, int fd_out) {
    int code;
    int data;
    while(1) {
        read(fd_in, in_packet, packet_size);
#ifdef DEBUG
        printf("SERVER: received request, code: %d, data: %d\n", in_packet->code, in_packet->data);
#endif
        if (in_packet->code != Q_CREATE && in_packet->code != EXIT && !server_stack) {
            form_packet(out_packet, E_STACK_NULL, 0);
            printf("SERVER: Error: stack not created, operation not performed\n");
            write(fd_out, out_packet, packet_size);
            continue;
        }
        switch (in_packet->code) {
            case Q_CREATE:
                create();
                code = OK;
                data = 0;
                printf("SERVER: create ok\n");
                break;
            case Q_PEEK:
                data = peek();
                if (stack_err == CODE_STACK_EMPTY) {
                    code = E_STACK_EMPTY;
                    printf("SERVER: Error: stack is empty\n");
                } else {
                    code = OK;
                    printf("SERVER: peek: %d\n", data);
                }
                break;
            case Q_PUSH:
                push(in_packet->data);
                code = OK;
                data = 0;
                printf("SERVER: push ok\n");
                break;
            case Q_POP:
                pop();
                data = 0;
                if (stack_err == CODE_STACK_EMPTY) {
                    code = E_STACK_EMPTY;
                    printf("SERVER: Error: stack is empty\n");
                } else {
                    code = OK;
                    printf("SERVER: pop: ok\n");
                }
                break;
            case Q_EMPTY:
                code = OK;
                data = empty();
                if (data) {
                    printf("SERVER: stack is empty\n");
                } else {
                    printf("SERVER: stack is not empty\n");
                }
                break;
            case Q_STACKSIZE:
                code = OK;
                data = server_stack_size();
                printf("SERVER: stack_size: %d\n", data);
                break;
            case Q_DISPLAY:
                code = OK;
                data = 0;
                display();
                break;
            case EXIT:
                form_packet(out_packet, OK, 0);
                write(fd_out, out_packet, packet_size);
                printf("SERVER: closing...\n");
                exit(0);
            default:
                printf("SERVER: received unknown packet\n");
                code = ERROR;
                data = 0;
        }
        form_packet(out_packet, code, data);
        write(fd_out, out_packet, packet_size);
#ifdef DEBUG
        printf("SERVER: response sent\n");
#endif
    }
}
// Client implementation

void request_response(int code, int data, int fd_in, int fd_out, int pid) {
    form_packet(out_packet, code, data);
    write(fd_out, out_packet, packet_size);
    kill(pid, SIGCONT);
    read(fd_in, in_packet, packet_size);
    kill(pid, SIGSTOP);
}

void print_help() {
    printf("CLIENT: Commands:\n");
    printf("CLIENT: help, peek, push [number], pop, empty, display, create, stack_size, exit, close\n");
}

void client_routine(int fd_in, int fd_out, int pid) {
    kill(pid, SIGSTOP);
    size_t packet_size = sizeof(packet);
    size_t bufsize;
    bufsize = READ_SIZE;
    char* input_string;
    input_string = (char*) malloc(sizeof(char) * (bufsize+1));
    int string_len;
    int code;
    int data;
    print_help();
    while (1) {
        printf("-----------------\n");
        printf("CLIENT: Enter command: ");
        //getline(&input_string, &bufsize, stdin);
        if (!fgets(input_string, (int) bufsize, stdin)) {
            printf("CLIENT: sending exit command to server...\n");
            request_response(EXIT, 0, fd_in, fd_out, pid);
            printf("CLIENT: closing...\n");
            exit(0);
        }
        string_len = (int) strlen(input_string);
        input_string[--string_len] = 0;
        if (!(strcmp(input_string, "exit") && strcmp(input_string, "close"))) {
#ifdef DEBUG
            printf("CLIENT: sending exit command to server...\n");
#endif
            request_response(EXIT, 0, fd_in, fd_out, pid);
            printf("CLIENT: closing...\n");
            exit(0);
        }
        if (!strcmp(input_string, "help")) {
            print_help();
            continue;
        } else if (!strcmp(input_string, "push")) {
            printf("CLIENT: push: usage: push [number]\n");
            continue;
        } else if (!strcmp(input_string, "peek")) {
            code = Q_PEEK;
            data = 0;
            request_response(code, data, fd_in, fd_out, pid);
        } else if (!strncmp(input_string, "push ", 5)) {
            code = Q_PUSH;
            data = (int) strtol(input_string+5, NULL, 10);
            request_response(code, data, fd_in, fd_out, pid);
        } else if (!strcmp(input_string, "pop")) {
            code = Q_POP;
            data = 0;
            request_response(code, data, fd_in, fd_out, pid);
        } else if (!strcmp(input_string, "empty")) {
            code = Q_EMPTY;
            data = 0;
            request_response(code, data, fd_in, fd_out, pid);
        } else if (!strcmp(input_string, "display")) {
            code = Q_DISPLAY;
            data = 0;
            request_response(code, data, fd_in, fd_out, pid);
        } else if (!strcmp(input_string, "create")) {
            code = Q_CREATE;
            data = 0;
            request_response(code, data, fd_in, fd_out, pid);
        } else if (!strcmp(input_string, "stack_size")) {
            code = Q_STACKSIZE;
            data = 0;
            request_response(code, data, fd_in, fd_out, pid);
        } else if (!strcmp(input_string, "")) {
            continue;
        } else {
            printf("CLIENT: unknown command: \"%s\"\n", input_string);
            continue;
        }
        if (in_packet->code == E_STACK_NULL) {
            printf("CLIENT: Error: stack not created, operation not performed\n");
        } else if (in_packet->code == E_STACK_EMPTY) {
            printf("CLIENT: Error: stack is empty, operation not performed\n");
        } else {
            printf("CLIENT: Operation performed with response %d\n", in_packet->data);
        }
#ifdef DEBUG
        printf("CLIENT: received response, code: %d, data: %d\n", in_packet->code, in_packet->data);
#endif
    }
}

int main() {
    int request[2];
    int response[2];
    pipe(request);
    pipe(response);
    in_packet = malloc(packet_size);
    out_packet = malloc(packet_size);
    int pid = fork();
    if(pid < 0) {
        printf("Error while fork\n");
        return 1;
    }
    if (pid) { // Parent, client
        close(request[0]);
        close(response[1]);
        client_routine(response[0], request[1], pid);
    } else { // Child, Server
        close(request[1]);
        close(response[0]);
        server_routine(request[0], response[1]);
    }
}
