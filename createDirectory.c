

//FUNKCJA POWODUJE BŁĘDY I TERMINACJĘ DEMONA
void createDirectories(char* path) {
    char* pathCopy = strdup(path);  // Kopiujemy ścieżkę do osobnej zmiennej, aby nie modyfikować oryginalnej
    char* token = strtok(pathCopy, "/");  // Rozdzielamy ścieżkę na poszczególne katalogi
    
    char currentPath[256] = "";  // Inicjalizujemy pusty aktualny katalog
    
    while (token != NULL) {
        // Dodajemy kolejny katalog do aktualnej ścieżki
        strcat(currentPath, token);
        strcat(currentPath, "/");
        
        // Tworzymy katalog, jeśli nie istnieje
        struct stat st;
        if (stat(currentPath, &st) == -1) {
            mkdir(currentPath, 0700);  // Uprawnienia dla nowo utworzonego katalogu
        }
        
        token = strtok(NULL, "/");  // Przechodzimy do następnego katalogu
    }
    
    free(pathCopy);  // Zwolnienie pamięci
}
