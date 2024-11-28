#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){
    FILE *arq;
    char *result;
    int comp;
    char Linha[100];
    int i;
    char Texto[100];

    // Leitura
    
    arq = fopen("input.txt", "rt");

    if (arq == NULL)
    {
        printf("Problemas na abertura do arquivo\n");
        return;
    }

    i = 1;
    while (!feof(arq))
    {
        result = fgets(Linha, 100, arq);  // Lê uma linha ou ate 99 caracteres
        if (result)  // Verifica se foi possível ler
        printf("Linha %d : %s",i,Linha);
        i++;
    }
    printf("\n***** Fim do arquivo *****\n");
    fclose(arq);


    // Escrita

    strcpy(Texto, "Escrevendo no arquivo\nHello World!");

    arq = fopen("output.txt", "wt");  // Cria um arquivo texto para gravação
    if (arq == NULL) // Se não conseguiu criar
    {
    printf("Problemas na CRIACAO do arquivo\n");
    return;
    }

    comp = fputs(Texto, arq);
    if (comp == EOF)
        printf("Erro na Gravacao\n");
    fclose(arq);

    return 0;
}