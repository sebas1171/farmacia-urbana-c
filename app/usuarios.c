#include <stdio.h>
#include <string.h>
#include "usuarios.h"

static struct Usuario usuarios[] = {
    {"admin", "admin123", "ADMIN"},
    {"vendedor1", "vend123", "VENDEDOR"},
    {"vendedor2", "vend456", "VENDEDOR"}
};

static const int totalUsuarios = sizeof(usuarios) / sizeof(usuarios[0]);

void iniciarSesion(const char *username, const char *password) {
    int i;

    if (username == 0 || password == 0) {
        printf("LOGIN_ERROR");
        return;
    }

    for (i = 0; i < totalUsuarios; i++) {
        if (strcmp(username, usuarios[i].username) == 0 &&
            strcmp(password, usuarios[i].password) == 0) {
            if (strcmp(usuarios[i].rol, "ADMIN") == 0) {
                printf("LOGIN_EXITO|ADMIN");
            } else {
                printf("LOGIN_EXITO|VENDEDOR");
            }
            return;
        }
    }

    printf("LOGIN_ERROR");
}
