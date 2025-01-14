#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <glob.h>
#include <signal.h>

// Taille maximale pour une ligne de commande
#define TAILLE_MAX_COMMANDE 1024

// Déclaration de la variable globale environ
extern char **environ;

// Prototypes des fonctions
void changer_repertoire(char *chemin);
void afficher_repertoire_courant();
void executer_commande(char **arguments, int en_arriere_plan);
void analyser_et_executer_ligne(char *ligne);
char* remplacer_variable(char *commande);
void remplacer_motifs(char *commande);
char *chercher_commande_dans_path(char *commande);
void gestionnaire_sigchld(int sig);

// Fonction pour remplacer une variable d'environnement par sa valeur
char* remplacer_variable(char *commande) {
    if (commande[0] == '$') {
        char *variable = getenv(commande + 1); // Récupérer la variable d'environnement sans le '$'
        if (variable != NULL) {
            printf("[INFO] Remplacement de la variable d'environnement : %s\n", commande);
            return variable; // Retourner la valeur de la variable
        } else {
            printf("[ERROR] Variable d'environnement '%s' non trouvée.\n", commande);
            return ""; // Retourner une chaîne vide si la variable n'existe pas
        }
    }
    return commande; // Si ce n'est pas une variable, renvoyer la commande originale
}

// Fonction pour remplacer les caractères spéciaux (comme * ou ?) par les correspondances de fichiers
void remplacer_motifs(char *commande) {
    glob_t glob_res;
    if (glob(commande, 0, NULL, &glob_res) == 0) {
        if (glob_res.gl_pathc > 0) {
            strcpy(commande, glob_res.gl_pathv[0]);
            printf("[INFO] Remplacement des motifs : %s\n", commande);
        }
    }
    globfree(&glob_res);
}

// Fonction pour chercher la commande dans les répertoires de PATH
char *chercher_commande_dans_path(char *commande) {
    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        printf("[ERROR] La variable d'environnement PATH est absente.\n");
        return NULL;
    }

    char *path_copy = strdup(path_env); // Copie de PATH pour le découper
    char *repertoire = strtok(path_copy, ":");

    while (repertoire != NULL) {
        // Concaténer le répertoire avec la commande
        char *chemin = malloc(TAILLE_MAX_COMMANDE);
        snprintf(chemin, TAILLE_MAX_COMMANDE, "%s/%s", repertoire, commande);

        // Vérifier si le fichier existe et est exécutable
        if (access(chemin, X_OK) == 0) {
            free(path_copy);
            return chemin; // Retourner le chemin complet de la commande
        }

        free(chemin);
        repertoire = strtok(NULL, ":");
    }

    free(path_copy);
    printf("[ERROR] Commande '%s' introuvable dans PATH.\n", commande);
    return NULL; // Commande non trouvée
}

// Fonction pour exécuter une commande avec execve
void executer_commande(char **arguments, int en_arriere_plan) {
    pid_t pid = fork();
    if (pid == 0) {
        // Processus enfant
        char *chemin_commande = chercher_commande_dans_path(arguments[0]);

        if (chemin_commande != NULL) {
            // Remplacer les variables et motifs dans les arguments
            for (int i = 0; arguments[i] != NULL; i++) {
                arguments[i] = remplacer_variable(arguments[i]);
                remplacer_motifs(arguments[i]);
            }

            if (execve(chemin_commande, arguments, environ) == -1) {
                printf("[ERROR] Impossible d'exécuter la commande '%s' (%s).\n", arguments[0], strerror(errno));
            }

            free(chemin_commande); // Libérer la mémoire allouée pour le chemin
        } else {
            printf("[ERROR] Commande '%s' introuvable.\n", arguments[0]);
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        printf("[ERROR] Échec de la création du processus (%s).\n", strerror(errno));
    } else {
        // Processus parent
        if (en_arriere_plan) {
            printf("[INFO] Exécution en arrière-plan : PID %d, Commande : %s\n", pid, arguments[0]);
        } else {
            waitpid(pid, NULL, 0);
        }
    }
}

// Fonction pour analyser et exécuter une ligne contenant plusieurs commandes séparées par ;
void analyser_et_executer_ligne(char *ligne) {
    char *commande = strtok(ligne, ";");
    while (commande != NULL) {
        // Supprimer les espaces au début et à la fin
        while (*commande == ' ') commande++;
        commande[strcspn(commande, "\n")] = '\0';

        if (strlen(commande) > 0) {
            char *arguments[64];
            char *token = strtok(commande, " ");
            int en_arriere_plan = 0, i = 0;

            while (token != NULL) {
                if (strcmp(token, "&") == 0) {
                    en_arriere_plan = 1;
                } else {
                    arguments[i++] = token;
                }
                token = strtok(NULL, " ");
            }
            arguments[i] = NULL;

            if (arguments[0] == NULL) continue; // Commande vide
            if (strcmp(arguments[0], "cd") == 0) {
                if (arguments[1] != NULL) {
                    changer_repertoire(arguments[1]);
                } else {
                    printf("[ERROR] Veuillez spécifier un répertoire avec 'cd'.\n");
                }
            } else if (strcmp(arguments[0], "pwd") == 0) {
                afficher_repertoire_courant();
            } else if (strcmp(arguments[0], "exit") == 0) {
                printf("[INFO] Sortie de mbash. À bientôt !\n");
                exit(0);
            } else if (strcmp(arguments[0], "echo") == 0) {
                for (int i = 1; arguments[i] != NULL; i++) {
                    char *valeur = remplacer_variable(arguments[i]);
                    printf("[RESULT] %s ", valeur);
                }
                printf("\n");
            } else if (strcmp(arguments[0], "set") == 0) {
                // Exemple : set NOM_VARIABLE valeur
                if (arguments[1] != NULL && arguments[2] != NULL) {
                    setenv(arguments[1], arguments[2], 1); // Définir la variable sans $
                    printf("[INFO] Variable d'environnement '%s' définie à '%s'.\n", arguments[1], arguments[2]);
                } else {
                    printf("[ERROR] Utilisation de 'set' incorrecte.\n");
                }
            } else {
                executer_commande(arguments, en_arriere_plan);
            }
        }

        commande = strtok(NULL, ";");
    }
}

// Fonction pour changer de répertoire
void changer_repertoire(char *chemin) {
    if (chemin == NULL) {
        printf("[ERROR] Veuillez spécifier un répertoire.\n");
    } else if (chdir(chemin) == -1) {
        printf("[ERROR] Impossible d'accéder au répertoire '%s' (%s).\n", chemin, strerror(errno));
    } else {
        printf("[INFO] Changement de répertoire vers '%s'.\n", chemin);
    }
}

// Fonction pour afficher le répertoire courant
void afficher_repertoire_courant() {
    char chemin[TAILLE_MAX_COMMANDE];
    if (getcwd(chemin, sizeof(chemin)) != NULL) {
        printf("[RESULT] Répertoire courant : %s\n", chemin);
    } else {
        printf("[ERROR] Impossible de récupérer le répertoire courant (%s).\n", strerror(errno));
    }
}

// Gestionnaire de signaux pour SIGCHLD
void gestionnaire_sigchld(int sig) {
    (void)sig; // Éviter les avertissements pour le paramètre inutilisé
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Récupérer les processus enfants terminés
    }
}

// Fonction principale
int main() {
    char ligne[TAILLE_MAX_COMMANDE];
    char *ps1 = getenv("PS1");

    if (!ps1) {
        ps1 = "mbash> "; // Valeur par défaut si PS1 n'est pas défini
    }

    // Configurer le gestionnaire pour SIGCHLD
    struct sigaction sa;
    sa.sa_handler = gestionnaire_sigchld;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Réactiver les appels système interrompus
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("[ERROR] Impossible de configurer SIGCHLD");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Affichage du prompt
        printf("%s", ps1);
        fflush(stdout);

        // Lecture de la commande
        if (fgets(ligne, TAILLE_MAX_COMMANDE, stdin) == NULL) {
            printf("[INFO] Quitter mbash\n");
            break;
        }

        // Supprimer les espaces inutiles et analyse de la ligne
        char *ligne_trimmed = ligne;
        while (*ligne_trimmed == ' ') ligne_trimmed++;

        if (*ligne_trimmed != '\0') {
            analyser_et_executer_ligne(ligne_trimmed);
        }
    }

    return 0;
}
