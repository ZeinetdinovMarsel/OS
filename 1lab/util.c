#include "util.h"

void tokenize(char* input, char* tokens[], int* tokenCount) {
    *tokenCount = 0;
    char *token = strtok(input, " \t\n\r");

    while (token != NULL) {
        tokens[(*tokenCount)++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    
    tokens[*tokenCount] = NULL;
}
