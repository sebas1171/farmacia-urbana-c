#ifndef USUARIOS_H
#define USUARIOS_H

#define MAX_USERNAME 32
#define MAX_PASSWORD 32
#define MAX_ROL 16

struct Usuario {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char rol[MAX_ROL];
};

void iniciarSesion(const char *username, const char *password);

#endif
