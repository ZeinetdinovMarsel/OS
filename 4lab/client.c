#include <stdio.h>
#include <unistd.h>
#include "threadpool.h"

struct data
{
    int a;
    int b;
};

void add(void *param)
{
    struct data *temp;
    temp = (struct data *)param;
    printf("I add two values %d and %d result = %d\n", temp->a, temp->b, temp->a + temp->b);
}

void sub(void *param)
{
    struct data *temp;
    temp = (struct data *)param;
    printf("I sub from value %d value %d result = %d\n", temp->a, temp->b, temp->a - temp->b);
}

void mul(void *param)
{
    struct data *temp;
    temp = (struct data *)param;
    printf("I mul two values %d and %d result = %d\n", temp->a, temp->b, temp->a * temp->b);
}

void div(void *param)
{
    struct data *temp;
    temp = (struct data *)param;
    printf("I div value %d by value %d result = %d\n", temp->a, temp->b, temp->a / temp->b);
}
void inc(void *param)
{
    struct data *temp;
    temp = (struct data *)param;
    for (int i = 0; temp->a < 1000000; i++)
    {
        printf("Inc value %d inc value %d\n", temp->a++, temp->b++);
    }
}
int main(void)
{
    // create some work to do
    struct data work, work1, work2, work3;
    work.a = 0;
    work.b = 0;
    work1.a = 55;
    work1.b = 100;
    work2.a = 33;
    work2.b = 22;
    work3.a = 99;
    work3.b = 77;

    // initialize the thread pool
    pool_init();

    // submit the work to the queue
    pool_submit(&add, &work);
    pool_submit(&sub, &work);
    // pool_submit(&mul, &work);
    // pool_submit(&div, &work);


    pool_submit(&add, &work1);
    pool_submit(&sub, &work1);
    // pool_submit(&mul, &work1);
    // pool_submit(&div, &work1);

    // pool_submit(&add, &work2);
    pool_submit(&sub, &work2);
    // pool_submit(&mul, &work2);
    // pool_submit(&div, &work2);

    pool_submit(&add, &work3);
    pool_submit(&sub, &work3);
    // pool_submit(&mul, &work3);
    // pool_submit(&div, &work3);
    // // may be helpful
    // sleep(3);

    // shutdown the thread pool
    pool_shutdown();
    return 0;
}