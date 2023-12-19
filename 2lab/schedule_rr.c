#include "schedulers.h"
#include "list.h"
#include "stdlib.h"
#include "cpu.h"
#include "sys/param.h"

struct node* tasks = NULL;
struct node* last_node = NULL;

void add(Task* task) {
    insert(&tasks, task);
    if (last_node == NULL){
        last_node = tasks;
    } 
}

Task* pick_next() {
    if (tasks != tasks->next){
        if (tasks->task->burst == 0){
            tasks->prev->next = tasks->next;
            tasks->next->prev = tasks->prev;
        }
        tasks = tasks->next;
        return tasks->task;
    }
    return NULL;
}

void schedule() {
    last_node->next = tasks;
    tasks->prev = last_node;
    tasks = last_node;

    while (pick_next() != NULL) {
        run(tasks->task, MIN(tasks->task->burst, TIME_QUANTUM));
        tasks->task->burst -= MIN(tasks->task->burst, TIME_QUANTUM);
    }
}
