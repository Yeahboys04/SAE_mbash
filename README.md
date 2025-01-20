# Rapport de Projet: `mbash`

- Ryan Korban Vivein Herman S3-C

## 1. Partie 1: Création de `mbash`

### 1.1 Objectif du projet

Le but de cette partie était de créer une version simplifiée de Bash, appelée `mbash`. :

### 1.2 Fonctionnalités de `mbash`

Les fonctionnalités principales que nous avons implémentées dans `mbash` comprennent :

1. **Commande `pwd`**: Affiche le répertoire courant.

   ```bash
   mbash> pwd
   /home/utilisateur
   ```

2. **Commande `cd`**: Permet de naviguer dans les répertoires.

   - Si le répertoire est incorrect :
     ```bash
     mbash> cd /chemin/incorrect
     Erreur cd : impossible d'accéder au répertoire '/chemin/incorrect' (No such file or directory).
     ```
   - Si le répertoire est correct :
     ```bash
     mbash> cd /home
     mbash> pwd
     /home
     ```

3. **Variables d'environnement**:

   ```bash
   mbash> echo $HOME
   /home/utilisateur

    mbash> set MON_VAR test_value
    mbash> echo $MON_VAR
    test_value
   ```

4. **Exécution de commandes externes**: Utilisation de commandes système comme `ls`, `cat`, etc. (tout les paramètres sont pris en charge grace à `execve()` qui prend en argument un tableau d'arguments). :

   ```bash
   mbash> ls
   Documents  Téléchargements  Musique  etc.

   mbash> cat fichier.txt
    Contenu du fichier.txt

    mbash> ls -l
    total 4
    -rw-r--r-- 1 utilisateur utilisateur 0 Sep  1 12:00 fichier1.txt
    -rw-r--r-- 1 utilisateur utilisateur 0 Sep  1 12:00 fichier2.txt
    drwxr-xr-x 1 utilisateur utilisateur 0 Sep  1 12:00 dossierA
    drwxr-xr-x 1 utilisateur utilisateur 0 Sep  1 12:00 dossierB
   ```

5. **Gestion de toutes les autres commandes**: Gestion de toutes les autres commandes tel que ps mkdir touch nano etc... (tout marche) en utilisant `execve()`.

6. **Gestion des processus en arrière-plan**: Exécution de commandes comme `sleep` en arrière-plan :

   ```bash
   mbash> sleep 10 &
   [PID 1234] sleep
   ```

7. **Gestion des processus enfants**: Nous avons utilisé `sigaction` pour gérer la réception des signaux `SIGCHLD` lorsque les processus enfants terminent leur exécution pour éviter des processus zombie.

## 2. Partie 2: Serveur de Paquets Debian pour `mbash`

### 2.1 Installation des outils nécessaires sur le serveur

Sur le serveur, il faut d'abord installer les outils nécessaires pour créer et gérer un dépôt Debian.

```bash
sudo apt update
sudo apt install -y dpkg-dev reprepro dpkg-sig gnupg apache2
```

### 2.2 Configuration du dépôt avec `reprepro`

Créez le répertoire de votre dépôt Debian et configurez `reprepro` pour qu'il gère ce dépôt.

1. Créez les répertoires nécessaires pour le dépôt :

   ```bash
   mkdir -p ~/debian_repo/conf
   cd ~/debian_repo
   ```

2. Créez le fichier de configuration `distributions` pour `reprepro` :
   ```bash
   cat <<EOF > conf/distributions
   Origin: RyanVivien
   Label: CustomRepo
   Suite: stable
   Codename: stable
   Architectures: amd64
   Components: main
   Description: Mini bash pour une sae
   SignWith: yes
   EOF
   ```

### 2.3 Génération de la clé GPG

Générez une clé GPG pour signer les paquets.

```bash
gpg --full-generate-key
```

Une fois la clé générée, exportez la clé publique pour le client :

```bash
gpg --armor --export ryanvivien@gmail.com> > ~/public.key
```

# Déploiement de mbash avec un serveur Debian

## 2. Partie Serveur

### 2.4 Création du package Debian pour mbash

Nous avons créé le répertoire pour le programme mbash et le package Debian.

1. Nous avons créé les répertoires nécessaires pour le package :

   ```bash
   mkdir -p ~/mbash-0.1/usr/bin
   mkdir -p ~/mbash-0.1/DEBIAN
   ```

2. Nous avons ajouté le programme compilé dans le répertoire `~/mbash-0.1/usr/bin` (nous avons vérifié que notre programme mbash était bien compilé et placé ici).

3. Nous avons créé le fichier de contrôle `DEBIAN/control` pour le package :

   ```bash
   cat <<EOF > ~/mbash-0.1/DEBIAN/control
   Package: mbash
   Version: 0.1
   Section: base
   Priority: optional
   Architecture: amd64
   Maintainer: VotreNom <votre-email@example.com>
   Description: Mini programme Bash
   EOF
   ```

4. Nous avons construit le package Debian :

   ```bash
   dpkg-deb --build ~/mbash-0.1
   ```

5. Nous avons vérifié le package :

   ```bash
   dpkg --info ~/mbash-0.1.deb
   ```

### 2.5 Ajout du package au dépôt

Nous avons ajouté le package au dépôt avec la commande suivante :

```bash
reprepro -b ~/debian_repo includedeb stable ~/mbash-0.1.deb
```

### 2.6 Configuration du serveur Apache

Nous avons créé un lien symbolique pour rendre le dépôt accessible via HTTP avec Apache :

```bash
sudo ln -s ~/debian_repo /var/www/html/Debian
```

Ensuite, nous avons vérifié que le dépôt était accessible dans le navigateur à l'adresse suivante :

```bash
http://localhost/debian
```

---

## 3. Partie Client

### 3.1 Ajout de la clé publique

Sur le client, nous avons ajouté la clé publique générée sur le serveur pour authentifier les paquets :

```bash
sudo apt-key add ~/public.key
```

### 3.2 Ajout du dépôt à la liste des sources

Nous avons modifié le fichier `/etc/apt/sources.list` pour ajouter le dépôt que nous avons configuré sur le serveur. Nous avons remplacé `<server-ip>` par l'adresse IP du serveur :

```bash
echo "deb [arch=amd64] http://<ip du serveur ici>/debian stable main" | sudo tee -a /etc/apt/sources.list
```

### 3.3 Mise à jour de la base de données des paquets

Nous avons mis à jour la base des paquets pour prendre en compte le nouveau dépôt :

```bash
sudo apt update
```

### 3.4 Installation de mbash

Maintenant, nous avons installé mbash en utilisant la commande suivante :

```bash
sudo apt install mbash
```

---

## 4. Mise à jour du logiciel (Cycle de vie)

Si on veut publier une nouvelle version de `mbash` (par exemple, version 0.2), on suit ces étapes sur le serveur :

1. D'abord on modifie la version dans le fichier `~/mbash-0.2/DEBIAN/control`.
2. Puis on reconstruit le package et on l'ajoute au dépôt :

   ```bash
   dpkg-deb --build ~/mbash-0.2
   reprepro -b ~/debian_repo includedeb stable ~/mbash-0.2.deb
   ```

3. Sur le client, il faut mettre à jour la base des paquets et les installer la nouvelle version :

   ```bash
   sudo apt update
   sudo apt upgrade
   ```

---

En suivant toutes ces étapes on a réussi à mettre en place le serveur et le client pour le déploiement de `mbash` sur un réseau local.
