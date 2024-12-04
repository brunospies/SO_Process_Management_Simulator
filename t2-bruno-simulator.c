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
    int nDisp;
    int *tCpu;
    int *devices;
    int num_cycles;
    int state; 
    int remaining_time;
    int waiting_time;
    int throughput_time;
    int device_wait;
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
        processes[i-1]->tCpu = (int *)malloc(cols[i] / 2 * sizeof(int));
        processes[i-1]->devices = (int *)malloc((cols[i] / 2 - 1) * sizeof(int));
        devices = processes[i-1]->devices;
        tCpu = processes[i-1]->tCpu;
        processes[i-1]->state = -1;
        processes[i-1]->num_cycles = 0;
        processes[i-1]->waiting_time = 0;
        processes[i-1]->throughput_time = 0;
        processes[i-1]->nDisp=0;
        processes[i-1]->device_wait=0;
        processes[i-1]->remaining_time = TIME_QUANTUM;

        for (j=0;j<MAX_DEVICES;j++) processes[i-1]->device_time[j] = 0;

        for (j = 1; j < cols[i]; j++) {
            if (j%2==0){
                *devices = input_data[i][j];
                processes[i-1]->nDisp++;
                devices++;
            }else{
                *tCpu = input_data[i][j];
                tCpu++;
            }
        }
    }

    for (i = 0; i < rows; i++) {
    if (input_data[i]) {
        free(input_data[i]);
    }
}
}

void printSimulationState(int time, PCB *processes[], int nProc, FILE *output) {
    fprintf(output, "<%02d>", time);

    int i;
    for (i = 0; i < nProc; i++) {
        char state[50]; 

        switch (processes[i]->state) {
            case 0: 
                strcpy(state, "new/ready        ");
                break;
            case 1: 
                strcpy(state, "ready            ");
                break;
            case 2: 
                strcpy(state, "running          ");
                break;
            case 3: 
                sprintf(state, "blocked d%d       ", processes[i]->device_wait+1);
                break;
            case 4: 
                strcpy(state, "terminated       ");
                break;
            case 5: 
                sprintf(state, "bloqued queue d%d ", processes[i]->device_wait+1);
                break;
            default: 
                strcpy(state, "--               ");
        }

        // Escreve o estado no arquivo de saída
        fprintf(output, " | P%02d state: %s", processes[i]->id, state);
    }
    fprintf(output, " |\n");
}

void simulate(const char *input_file, const char *output_file) {
    PCB *processes[MAX_PROCESSES];
    int i, j, nProc, nDisp, tDisp[MAX_DEVICES];
    int *tCpu = NULL;
    int *devices = NULL;

    parseInputFile(input_file, processes, &nProc, &nDisp, tDisp);
    
    FILE *output = fopen(output_file, "w");
    if (!output) {
        perror("Erro ao abrir arquivo de saída");
        exit(EXIT_FAILURE);
    }

    Queue *readyQueue = createQueue();
    Queue *deviceQueues[MAX_DEVICES];

    for (i = 0; i < nDisp; i++) {
        deviceQueues[i] = createQueue();
        if (!deviceQueues[i]) {
            perror("Erro ao criar fila de dispositivos");
            exit(EXIT_FAILURE);
        }
    }

    int time = 0;
    int cpu_idle_time = 0;
    PCB *currentProcess = NULL;
    char all_terminated = 0;
    int cpu_in_use = 0;
    int device_in_use[MAX_DEVICES];
    int count_terminated;

    for (i=0;i<MAX_DEVICES;i++){
        device_in_use[i] = 0;
    }

    while (all_terminated==0) {
        for(i=0,count_terminated=0;i<nProc;i++){
            tCpu = processes[i]->tCpu;
            devices = processes[i]->devices;


            if(*tCpu==0 && processes[i]->nDisp>0 && processes[i]->remaining_time>0 && processes[i]->state == 2){
                enqueue(deviceQueues[*devices - 1], processes[i]);
                cpu_in_use = 0;
                if(device_in_use[*devices-1]==1) {
                    processes[i]->state = 5;
                    processes[i]->device_wait = *devices-1;
                }               
                processes[i]->devices++;
                processes[i]->nDisp--;
            }else if(*tCpu==0 && processes[i]->nDisp==0 && processes[i]->state == 2){
                processes[i]->state = 4;
                cpu_in_use = 0;
            }else if(*tCpu==0 && processes[i]->nDisp>0 && processes[i]->remaining_time==0 && processes[i]->state == 2){
                processes[i]->state = 1;
                enqueue(readyQueue, processes[i]);
                processes[i]->remaining_time = TIME_QUANTUM;
                cpu_in_use = 0;
            }else if(*tCpu>0 && processes[i]->remaining_time>0 && processes[i]->state == 2){
                processes[i]->num_cycles++;
                *tCpu = *tCpu - 1;
                processes[i]->remaining_time--;
            }else if(*tCpu>0 && processes[i]->remaining_time==0 && processes[i]->state == 2){
                enqueue(readyQueue, processes[i]);
                processes[i]->remaining_time = TIME_QUANTUM;
                processes[i]->state = 1;
                cpu_in_use = 0;
            }else if(processes[i]->state == 3 && processes[i]->remaining_time>0){
                processes[i]->remaining_time--;
                processes[i]->device_time[*(devices-1)-1]++;
            }else if (processes[i]->state == 3 && processes[i]->remaining_time==0) {
                processes[i]->state = 1;
                device_in_use[*(devices-1)-1] = 0;
                enqueue(readyQueue, processes[i]);
                processes[i]->tCpu++;
                processes[i]->remaining_time = TIME_QUANTUM;
            }else if (processes[i]->state == 5) processes[i]->waiting_time++;

            if(processes[i]->state==0 || processes[i]->state==1 || processes[i]->state==2 || processes[i]->state==3) processes[i]->throughput_time++;

            if(processes[i]->state==4) count_terminated++;
        }

        for (i=0;i<nDisp;i++){
            if (!isEmpty(deviceQueues[i]) && device_in_use[i]==0){
                currentProcess = dequeue(deviceQueues[i]);
                currentProcess->device_wait = i;
                currentProcess->device_time[i]++;
                currentProcess->state=3;
                currentProcess->remaining_time = tDisp[i]-1;
                device_in_use[i] = 1;
            }
        }

        if (isEmpty(readyQueue) && cpu_in_use == 0) cpu_idle_time++;
        else if (cpu_in_use==0){
            currentProcess = dequeue(readyQueue);
            currentProcess->state=2;
            tCpu = currentProcess->tCpu;
            if(*tCpu>0) *tCpu = *tCpu - 1;
            currentProcess->remaining_time--;
            cpu_in_use = 1;
        }

        for(i=0;i<nProc;i++){
            if (processes[i]->tInicio==time){
                enqueue(readyQueue, processes[i]);
                processes[i]->state = 0;
            }
        }
        if(count_terminated==nProc) all_terminated=1;
        if(time>300) break;
        
        printSimulationState(time, processes, nProc, output);
        time++;
    }
    for(i=0;i<nProc;i++){
        fprintf(output, "| P0%d ", i+1);
        for(j=0; j<nDisp; j++){
            fprintf(output, "device time d%d: %d, ", j+1, processes[i]->device_time[j]);
        }
        fprintf(output, "waiting time: %d, troughput: %d\n", processes[i]->waiting_time, processes[i]->throughput_time);
    }

    fprintf(output, "| CPU idle time: %d |\n", cpu_idle_time-1);
    fclose(output);
}

int main() {
    simulate("input.txt", "output.txt");
    return 0;
}
