#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "myshell.h"
#include "util.h"

extern char **__environ;

// Функция для выполнения внешней команды
void executeExternalCommand(char *tokens[], int tokenCount, char *inputFile, char *outputFile, bool appendOutput)
{
    bool background = false;

    // Проверяем, выполняется ли команда в фоновом режиме
    if (tokenCount > 0 && strcmp(tokens[tokenCount - 1], "&") == 0)
    {
        background = true;
        tokens[tokenCount - 1] = NULL;
    }

    // Создаем новый процесс
    pid_t childPid = fork();

    if (childPid == 0) // Дочерний процесс
    {
        // Перенаправляем ввод, если указан файл для ввода
        if (inputFile)
        {
            int inputFd = open(inputFile, O_RDONLY);
            int oldinputFd = dup(0);

            if (inputFd < 0)
            {
                fprintf(stderr, "Не удалось открыть файл для ввода!\n");
                exit(EXIT_FAILURE);
            }

            if (dup2(inputFd, STDIN_FILENO) == -1)
            {
                fprintf(stderr, "Не удалось перенаправить ввод!\n");
            }

            close(inputFd);
        }

        // Если команда выполняется в фоновом режиме
        if (background)
        {
            // Перенаправляем ввод/вывод/ошибки в /dev/null
            int devNullFd = open("/dev/null", O_RDWR);

            if (devNullFd < 0)
            {
                fprintf(stderr, "Не удалось получить доступ к /dev/null!\n");
                exit(EXIT_FAILURE);
            }

            if (!inputFile)
            {
                dup2(devNullFd, STDIN_FILENO);
            }

            if (!outputFile)
            {
                dup2(devNullFd, STDOUT_FILENO);
            }

            dup2(devNullFd, STDERR_FILENO);

            close(devNullFd);
        }
        else // Если команда не выполняется в фоновом режиме
        {
            // Заменяем текущий процесс на новую программу
            execvp(tokens[0], tokens);
            fprintf(stderr, "Не удалось выполнить программу!\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (childPid > 0) // Родительский процесс
    {
        if (!background)
        {
            // Ждем завершения дочернего процесса
            waitpid(childPid, NULL, 0);
        }
    }
    else // Ошибка при создании процесса
    {
        fprintf(stderr, "Не удалось создать процесс!\n");
    }
}

// Функция для выполнения команды "help"
void executeHelpCommand()
{
    FILE *lessPipe;

    // Создаем pipe для less
    lessPipe = popen("less", "w");
    if (lessPipe == NULL)
    {
        perror("Не удалось создать pipe для less.");
        return;
    }

    // Получаем путь к исполняемому файлу оболочки
    char *readmePath = strdup(getenv("SHELL"));

    // Удаляем имя исполняемого файла из пути
    char *match = strstr(readmePath, SHELL_NAME);

    if (match != NULL)
    {
        size_t len = strlen(SHELL_NAME);
        memmove(match, match + len, strlen(match + len) + 1);
    }

    // Открываем файл readme для чтения
    int readmeFileFd = open(strcat(readmePath, "readme"), O_RDONLY);

    int oldinputFd = dup(0);

    // Проверяем, удалось ли открыть файл
    if (readmeFileFd < 0)
    {
        fprintf(stderr, "Не удалось открыть файл для чтения!\n");
        if (pclose(lessPipe) == -1)
        {
            perror("Не удалось закрыть pipe для потока.");
            return;
        }
        return;
    }

    // Перенаправляем ввод на содержимое файла
    if (dup2(readmeFileFd, STDIN_FILENO) == -1)
    {
        fprintf(stderr, "Не удалось перенаправить ввод!\n");
        if (pclose(lessPipe) == -1)
        {
            perror("Не удалось закрыть pipe для потока.");
            return;
        }
        return;
    }

    // Читаем содержимое файла и пишем в pipe для less
    char buffer[MAX_INPUT_SIZE];
    size_t bytesReadCount;

    while ((bytesReadCount = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0)
    {
        if (fwrite(buffer, 1, bytesReadCount, lessPipe) != bytesReadCount)
        {
            perror("Не удалось записать данные в поток.");
            break;
        }
    }

    // Очищаем буфер ввода
    fflush(stdin);
    dup2(oldinputFd, STDIN_FILENO);
    close(readmeFileFd);

    // Закрываем pipe для less
    if (pclose(lessPipe) == -1)
    {
        perror("Не удалось закрыть pipe для потока.");
        return;
    }
}

// Функция для выполнения внутренних команд
bool executeInternalCommand(char *tokens[], int tokenCount, char *outputFile, bool appendOutput)
{
    if (tokenCount == 0)
    {
        return true;
    }

    // Команда "cd" - изменение текущей директории
    if (strcmp(tokens[0], "cd") == 0)
    {
        if (tokenCount == 1)
        {
            if (chdir(".") != 0)
            {
                fprintf(stderr, "Недопустимый путь.\n");
            }
        }
        else
        {
            if (chdir(tokens[1]) != 0)
            {
                fprintf(stderr, "Недопустимый путь.\n");
            }
            else
            {
                // Устанавливаем переменные окружения OLDPWD и PWD
                setenv("OLDPWD", getenv("PWD"), 1);
                setenv("PWD", getcwd(NULL, 0), 1);
            }
        }
        return true;
    }
    // Команда "clr" - очистка экрана
    else if (strcmp(tokens[0], "clr") == 0)
    {
        printf("\x1B[2J");
        printf("\x1B[H");

        return true;
    }
    // Команда "dir" - вывод списка файлов в текущей или указанной директории
    else if (strcmp(tokens[0], "dir") == 0)
    {
        struct dirent *entry;
        DIR *dir = ((tokenCount == 1 || (tokenCount > 1 && !!outputFile && tokens[1] == NULL)) ? opendir(".") : opendir(tokens[1]));

        if (dir == NULL)
        {
            fprintf(stderr, "Не удалось открыть директорию!\n");
            return true;
        }

        char *filenames[MAX_DIRECTORY_CONTENTS];
        int filesCnt = 0;

        while ((entry = readdir(dir)) != NULL)
        {
            filenames[filesCnt++] = strdup(entry->d_name);
        }

        closedir(dir);
        return true;
    }
    // Команда "environ" - вывод переменных окружения
    else if (strcmp(tokens[0], "environ") == 0)
    {
        char **env = __environ;
        while (*env)
        {
            printf("%s\n", *env);
            env++;
        }

        return true;
    }
    // Команда "echo" - вывод аргументов
    else if (strcmp(tokens[0], "echo") == 0)
    {
        for (int i = 1; i < tokenCount; i++)
        {
            if (tokens[i] == NULL)
            {
                break;
            }
            printf("%s%s", tokens[i], ((i == tokenCount - 1) ? "\n" : " "));
        }

        return true;
    }
    // Команда "pause" - ожидание нажатия клавиши Enter
    else if (strcmp(tokens[0], "pause") == 0)
    {
        printf("Нажмите Enter для продолжения...");
        getchar();

        return true;
    }
    // Команда "quit" - выход из оболочки
    else if (strcmp(tokens[0], "quit") == 0)
    {
        exit(EXIT_SUCCESS);
    }
    // Команда "help" - вывод справки
    if (strcmp(tokens[0], "help") == 0)
    {
        executeHelpCommand();
        return true;
    }

    return false;
}

// Переменные для хранения имени файла ввода, вывода и флага дозаписи
char *inputFile = NULL;
char *outputFile = NULL;
bool appendOutput = false;

// Обновление переменных ввода-вывода на основе аргументов командной строки
void updateInputOutput(char *tokens[], int tokenCount)
{
    inputFile = NULL;
    outputFile = NULL;
    for (int i = 0; i < tokenCount - 1; i++)
    {
        if (!inputFile && strcmp(tokens[i], "<") == 0)
        {
            if (i < tokenCount - 1)
            {
                inputFile = tokens[i + 1];
                tokens[i] = NULL;
            }
            else
            {
                fprintf(stderr, "Отсутствует файл ввода!\n");
                return;
            }
        }
        else if (!outputFile && strcmp(tokens[i], ">") == 0)
        {
            if (i < tokenCount - 1)
            {
                outputFile = tokens[i + 1];
                tokens[i] = NULL;
            }
            else
            {
                fprintf(stderr, "Отсутствует файл вывода!\n");
                return;
            }
        }
        else if (!outputFile && strcmp(tokens[i], ">>") == 0)
        {
            if (i < tokenCount - 1)
            {
                outputFile = tokens[i + 1];
                tokens[i] = NULL;
                appendOutput = true;
            }
            else
            {
                fprintf(stderr, "Отсутствует файл вывода!\n");
                return;
            }
        }
    }
}

// Обработка входной строки
void proccessInput(char input[], char *tokens[], int tokenCount)
{
    // Разбиваем строку на токены
    tokenize(input, tokens, &tokenCount);
    // Обновляем переменные ввода-вывода
    updateInputOutput(tokens, tokenCount);

    if (tokenCount > 0)
    {
        int outputFd = -1;
        int oldoutputFd = dup(STDOUT_FILENO);
        if (outputFile)
        {

            // Открываем файл вывода
            if (appendOutput)
            {
                outputFd = open(outputFile, O_CREAT | O_RDWR | O_APPEND, 0644);
            }
            else
            {
                outputFd = open(outputFile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            }

            // Проверяем, удалось ли открыть файл
            if (outputFd < 0)
            {
                fprintf(stderr, "Не удалось открыть файл вывода!\n");
                exit(EXIT_FAILURE);
            }

            // Перенаправляем вывод в файл
            if (dup2(outputFd, STDOUT_FILENO) == -1)
            {
                fprintf(stderr, "Не удалось перенаправить вывод!\n");
            }
        }

        // Выполняем внутреннюю или внешнюю команду
        if (!executeInternalCommand(tokens, tokenCount, outputFile, appendOutput))
        {
            executeExternalCommand(tokens, tokenCount, inputFile, outputFile, appendOutput);
        }

        if (!!outputFile)
        {
            // Сбрасываем буфер вывода и восстанавливаем стандартный вывод
            fflush(stdout);
            dup2(oldoutputFd, STDOUT_FILENO);
            close(outputFd);
        }
    }
}

// Главная функция
int main(int argc, char *argv[])
{
    // Если указан файл с командами в качестве аргумента
    if (argc == 2)
    {
        FILE *cmd_source = fopen(argv[1], "r");
        if (cmd_source == NULL)
        {
            fprintf(stderr, "Не удалось открыть файл с командами!\n");
            exit(EXIT_FAILURE);
        }

        char buffer[MAX_INPUT_SIZE];
        char *buf_tokens[MAX_TOKENS];
        int buf_tokenCount = 0;

        // Читаем команды из файла и обрабатываем их
        while (fgets(buffer, sizeof(buffer), cmd_source) != NULL)
        {
            proccessInput(buffer, buf_tokens, buf_tokenCount);
        }

        fclose(cmd_source);

        return EXIT_SUCCESS;
    }

    // Получаем текущую директорию
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL)
    {
        fprintf(stderr, "Cannot get current directory!\n");
        exit(EXIT_FAILURE);
    }

    // Выделяем память для строки с путем до файла оболочки
    char *shellPath = (char *)malloc(strlen(cwd) + strlen(SHELL_NAME) + 2);
    if (shellPath == NULL)
    {
        fprintf(stderr, "malloc failed!\n");
        free(cwd);
        exit(EXIT_FAILURE);
    }

    // Формируем путь к файлу оболочки и выставляем переменную окружения
    sprintf(shellPath, "%s/%s", cwd, SHELL_NAME);
    setenv("SHELL", shellPath, 1);

    free(cwd);
    free(shellPath);

    char input[MAX_INPUT_SIZE];
    char *tokens[MAX_TOKENS];
    int tokenCount = 0;

    while (true)
    {
        char *current_dir = getcwd(NULL, 0);
        printf("%s$ ", current_dir);
        free(current_dir);

        // Если не можем получить входную строку или достигнут конец файла
        if (fgets(input, MAX_INPUT_SIZE, stdin) == NULL)
        {
            break;
        }

        proccessInput(input, tokens, tokenCount);
    }

    return EXIT_SUCCESS;
}
