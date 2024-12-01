#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 100
#define MAX_DEVICES 10
#define TIME_QUANTUM 4

// Estrutura PCB (Process Control Block)
typedef struct {
    int id;
    int tInicio;
    int *tCpu;
    int *devices;
    int num_cycles;
    int current_cycle;
    int state; // 0: new/ready, 1: ready, 2: running, 3: blocked, 4: terminated
    int remaining_time;
    int waiting_time;
    int throughput_time;
    int device_time[MAX_DEVICES];
} PCB;

// Estrutura da Fila
typedef struct Node {
    PCB *process;
    struct Node *next;
} Node;

typedef struct {
    Node *front;
    Node *rear;
} Queue;

// Funções da fila
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

// Funções auxiliares
void parseInputFile(const char *filename, PCB *processes[], int *nProc, int *nDisp, int tDisp[]) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d %d", nProc, nDisp);

    int i;
    for (i = 0; i < *nDisp; i++) {
        fscanf(file, "%d", &tDisp[i]);
    }

    for (i = 0; i < *nProc; i++) {
        processes[i] = (PCB *)malloc(sizeof(PCB));
        processes[i]->id = i + 1;
        fscanf(file, "%d", &processes[i]->tInicio);

        processes[i]->tCpu = (int *)malloc(MAX_PROCESSES * sizeof(int));
        processes[i]->devices = (int *)malloc(MAX_PROCESSES * sizeof(int));
        processes[i]->num_cycles = 0;

        while (1) {
            int tCpu, device;
            if (fscanf(file, "%d", &tCpu) != 1) break;
            processes[i]->tCpu[processes[i]->num_cycles] = tCpu;

            if (fscanf(file, "%d", &device) != 1) {
                processes[i]->devices[processes[i]->num_cycles] = -1;
                processes[i]->num_cycles++;
                break;
            }

            processes[i]->devices[processes[i]->num_cycles] = device;
            processes[i]->num_cycles++;
        }

        processes[i]->state = 0; // New/ready
        processes[i]->current_cycle = 0;
        processes[i]->remaining_time = processes[i]->tCpu[0];
        processes[i]->waiting_time = 0;
        processes[i]->throughput_time = 0;
        memset(processes[i]->device_time, 0, sizeof(processes[i]->device_time));
    }

    fclose(file);
}

void printSimulationState(int time, PCB *processes[], int nProc, FILE *output) {
    fprintf(output, "<%02d>", time);

    int i;
    for (i = 0; i < nProc; i++) {
        char *state;
        switch (processes[i]->state) {
            case 0: state = "new/ready"; break;
            case 1: state = "ready"; break;
            case 2: state = "running"; break;
            case 3: state = "blocked"; break;
            case 4: state = "terminated"; break;
            default: state = "--";
        }
        fprintf(output, " | P%02d state: %s", processes[i]->id, state);
    }
    fprintf(output, " |\n");
}

// Simulador
void simulate(const char *input_file, const char *output_file) {
    PCB *processes[MAX_PROCESSES];
    int nProc, nDisp, tDisp[MAX_DEVICES];
    parseInputFile(input_file, processes, &nProc, &nDisp, tDisp);

    FILE *output = fopen(output_file, "w");
    if (!output) {
        perror("Erro ao abrir arquivo de saída");
        exit(EXIT_FAILURE);
    }

    Queue *readyQueue = createQueue();
    Queue *deviceQueues[MAX_DEVICES];

    int i;
    for (i = 0; i < nDisp; i++) {
        deviceQueues[i] = createQueue();
    }

    int time = 0;
    int cpu_idle_time = 0;
    PCB *currentProcess = NULL;

    while (1) {
        int all_terminated = 1;

        // Checagem de novos processos
        for (i = 0; i < nProc; i++) {
            if (processes[i]->tInicio == time && processes[i]->state == 0) {
                processes[i]->state = 1; // Ready
                enqueue(readyQueue, processes[i]);
            }
        }

        // Gerenciamento da CPU
        if (currentProcess) {
            currentProcess->remaining_time--;

            // Se o tempo da CPU acabou ou a fatia de tempo foi alcançada
            if (currentProcess->remaining_time == 0 || currentProcess->remaining_time <= TIME_QUANTUM) {
                if (currentProcess->current_cycle < currentProcess->num_cycles - 1) {
                    currentProcess->current_cycle++;
                    currentProcess->state = 3; // Bloqueado
                    enqueue(deviceQueues[currentProcess->devices[currentProcess->current_cycle] - 1], currentProcess);
                } else {
                    currentProcess->state = 4; // Terminado
                }
                currentProcess = NULL;
            }
        }

        // Se não há processo em execução, pegar o próximo da fila ready
        if (!currentProcess && !isEmpty(readyQueue)) {
            currentProcess = dequeue(readyQueue);
            currentProcess->state = 2; // Running
            currentProcess->remaining_time = (currentProcess->tCpu[currentProcess->current_cycle] < TIME_QUANTUM) 
                                                ? currentProcess->tCpu[currentProcess->current_cycle]
                                                : TIME_QUANTUM;
        } else if (!currentProcess) {
            cpu_idle_time++;
        }

        // Gerenciamento de dispositivos
        for (i = 0; i < nDisp; i++) {
            if (!isEmpty(deviceQueues[i])) {
                PCB *process = dequeue(deviceQueues[i]);
                process->state = 1; // Ready
                enqueue(readyQueue, process);
            }
        }

        // Verificação de término
        all_terminated = 1;
        for (i = 0; i < nProc; i++) {
            if (processes[i]->state != 4) {
                all_terminated = 0;
                break;
            }
        }

        // Saída do estado atual
        printSimulationState(time, processes, nProc, output);

        if (all_terminated) break;
        time++;

        // Segurança para evitar loops infinitos
        if (time > 10000) {
            fprintf(output, "Erro: Loop infinito detectado. Verifique os dados de entrada.\n");
            break;
        }
    }

    fprintf(output, "| CPU idle time: %d |\n", cpu_idle_time);
    fclose(output);
}

int main() {
    simulate("input.txt", "output.txt");
    return 0;
}
