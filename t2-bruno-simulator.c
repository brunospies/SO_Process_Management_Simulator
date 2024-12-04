#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 100
#define MAX_ROWS 100 
#define MAX_DEVICES 10
#define TIME_QUANTUM 4


typedef struct {
    int id;
    int tInicio;
    int *tCpu;
    int *devices;
    int num_cycles;
    int current_cycle;
    int state; 
    int remaining_time;
    int waiting_time;
    int throughput_time;
    int device_time[MAX_DEVICES];
} PCB;


typedef struct Node {
    PCB *process;
    struct Node *next;
} Node;

typedef struct {
    Node *front;
    Node *rear;
} Queue;

Queue *createQueue() {
    Queue *q = (Queue *)malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    return q;
}

int isEmpty(Queue *q) {
    return q->front == NULL;
}

void enqueue(Queue *q, PCB *process) {
    Node *temp = (Node *)malloc(sizeof(Node));
    temp->process = process;
    temp->next = NULL;
    if (q->rear) q->rear->next = temp;
    q->rear = temp;
    if (!q->front) q->front = temp;
}

PCB *dequeue(Queue *q) {
    if (isEmpty(q)) return NULL;
    Node *temp = q->front;
    PCB *process = temp->process;
    q->front = q->front->next;
    if (!q->front) q->rear = NULL;
    free(temp);
    return process;
}

void parseInputFile(const char *filename, PCB *processes[], int *nProc, int *nDisp, int tDisp[]) {
    int *input_data[MAX_ROWS];  // Ponteiros para armazenar cada linha dinamicamente
    int cols[MAX_ROWS];     // Vetor para armazenar o número de colunas de cada linha
    int i, j, rows = 0;
    int *tCpu = NULL;
    int *devices = NULL;
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    // Lendo o arquivo linha por linha
    char buffer[1024];  // Buffer para armazenar uma linha do arquivo
    while (fgets(buffer, sizeof(buffer), file) != NULL && rows < MAX_ROWS) {
        int *line = NULL;
        int num, count = 0;

        // Criar um array dinâmico para armazenar os números da linha
        char *ptr = buffer;  // Ponteiro para iterar pela linha
        while (sscanf(ptr, "%d", &num) == 1) {
            line = realloc(line, (count + 1) * sizeof(int));  // Expande o array dinamicamente
            if (line == NULL) {
                printf("Erro de memoria.\n");
                fclose(file);
            }
            line[count++] = num;  // Adiciona o número ao array
            while (*ptr != '\0' && *ptr != ' ') ptr++;  // Avança para o próximo número
            while (*ptr == ' ') ptr++;  // Pula espaços extras
        }

        input_data[rows] = line;  // Armazena a linha na matriz
        cols[rows] = count;   // Armazena o número de colunas
        rows++;
    }

    fclose(file);

    *nProc = input_data[0][0];
    *nDisp = input_data[0][1];

    for(i=2;i<(*nDisp+2);i++){
        tDisp[i-2] = input_data[0][i]; 
    }

    for (i = 1; i < *nProc+1; i++) {
        processes[i-1] = (PCB *)malloc(sizeof(PCB));
        if (!processes[i-1]) {
            perror("Erro ao alocar memoria para PCB");
            exit(EXIT_FAILURE);
        }
        processes[i-1]->id = i;
        processes[i-1]->tInicio = input_data[i][0];
        devices = processes[i-1]->devices;
        tCpu = processes[i-1]->tCpu;
        processes[i-1]->state = -1;
        processes[i-1]->num_cycles = 0;
        processes[i-1]->current_cycle = 0;
        processes[i-1]->waiting_time = 0;
        processes[i-1]->throughput_time = 0;

        for (j = 1; j < cols[i]; j++) {
            if (j%2==0){
                devices = input_data[i][j];
                devices++;
            }else{
                tCpu = input_data[i][j];
                tCpu++;
            }
        }
    }

    for (int i = 0; i < rows; i++) {
        free(input_data[i]);
    }
}

void printSimulationState(int time, PCB *processes[], int nProc, FILE *output) {
    fprintf(output, "<%02d>", time);

    int i;
    for (i = 0; i < nProc; i++) {
        char *state;
        switch (processes[i]->state) {
            case 0: state = "new/ready "; break;
            case 1: state = "ready     "; break;
            case 2: state = "running   "; break;
            case 3: state = "blocked   "; break;
            case 4: state = "terminated"; break;
            default: state = "--        ";
        }
        fprintf(output, " | P%02d state: %s", processes[i]->id, state);
    }
    fprintf(output, " |\n");
}

void simulate(const char *input_file, const char *output_file) {
    PCB *processes[MAX_PROCESSES];
    int i, nProc, nDisp, tDisp[MAX_DEVICES];
    parseInputFile(input_file, processes, &nProc, &nDisp, tDisp);
    
    for (i=0;i<nProc;i++){
         printf("Process: %d\n id: %d tInicio: %d num_cycles: %d, current cycle: %d state: %d remaining_time: %d waiting time %d\n\n", i, processes[i]->id, processes[i]->tInicio, processes[i]->num_cycles, processes[i]->current_cycle, processes[i]->state, processes[i]->remaining_time, processes[i]->waiting_time);
    }
    
    FILE *output = fopen(output_file, "w");
    if (!output) {
        perror("Erro ao abrir arquivo de saída");
        exit(EXIT_FAILURE);
    }

    Queue *readyQueue = createQueue();
    Queue *deviceQueues[MAX_DEVICES];

    for (i = 0; i < nDisp; i++) {
        deviceQueues[i] = createQueue();
    }

    int time = 0;
    int cpu_idle_time = 0;
    PCB *currentProcess = NULL;
    char all_terminated = 0;
    int cpu_in_use = 0;

    while (all_terminated==0) {
        for(i=0;i<nProc;i++){
            if ((processes[i]->state == 1 || processes[i]->state == 0) && cpu_in_use == 0){
                processes[i]->state = 2;
                cpu_in_use = 1;
            }
            if (processes[i]->tInicio==time) processes[i]->state = 0;
            
            if (processes[i]->state == 2){
                processes[i]->num_cycles++;
                processes[i]->current_cycle++;
            }

            if(processes[i]->current_cycle>TIME_QUANTUM){
                processes[i]->state = 1;
                cpu_in_use = 0;
                processes[i]->current_cycle = 0;
            }
        }
        printSimulationState(time, processes, nProc, output);
        time++;

        if(time>50) break;
    }

    fprintf(output, "| CPU idle time: %d |\n", cpu_idle_time);
    fclose(output);
    
}

int main() {
    simulate("input.txt", "output.txt");
    return 0;
}
