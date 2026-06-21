#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
#include <fstream>
#include <fcntl.h>

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int job_count = 1; // Keep track of the background job count

    while (true)
    {
        bool escaped = false;

        std::cout << "$ ";

        std::string command;
        if (!std::getline(std::cin, command))
            break;

        std::vector<std::string> args;
        std::string current;

        bool in_single_quote = false;
        bool in_double_quote = false;

        for (char c : command)
        {
            if (escaped)
            {
                if (in_double_quote)
                {
                    if (c == '"' || c == '\\')
                    {
                        current += c;
                    }
                    else
                    {
                        current += '\\';
                        current += c;
                    }
                }
                else
                {
                    current += c;
                }

                escaped = false;
            }
            else if (in_double_quote && c == '\\')
            {
                escaped = true;
            }
            else if (!in_single_quote && !in_double_quote && c == '\\')
            {
                escaped = true;
            }
            else if (c == '\'' && !in_double_quote)
            {
                in_single_quote = !in_single_quote;
            }
            else if (c == '"' && !in_single_quote)
            {
                in_double_quote = !in_double_quote;
            }
            else if (c == ' ' && !in_single_quote && !in_double_quote)
            {
                if (!current.empty())
                {
                    args.push_back(current);
                    current.clear();
                }
            }
            else
            {
                current += c;
            }
        }

        if (!current.empty())
        {
            args.push_back(current);
        }

        if (args.empty())
            continue;

        // Check if the command should run in the background
        bool is_background = false;
        if (args.back() == "&")
        {
            is_background = true;
            args.pop_back(); // Remove the '&' from arguments
        }

        if (args.empty())
            continue;

        std::string outputFile;
        std::string errorFile;

        bool redirectStdout = false;
        bool redirectStderr = false;

        bool appendStdout = false;
        bool appendStderr = false;

        for (size_t i = 0; i + 1 < args.size();)
        {
            if (args[i] == ">" || args[i] == "1>")
            {
                redirectStdout = true;
                appendStdout = false;
                outputFile = args[i + 1];

                args.erase(args.begin() + i, args.begin() + i + 2);
            }
            else if (args[i] == ">>" || args[i] == "1>>")
            {
                redirectStdout = true;
                appendStdout = true;
                outputFile = args[i + 1];

                args.erase(args.begin() + i, args.begin() + i + 2);
            }
            else if (args[i] == "2>")
            {
                redirectStderr = true;
                appendStderr = false;
                errorFile = args[i + 1];

                args.erase(args.begin() + i, args.begin() + i + 2);
            }
            else if (args[i] == "2>>")
            {
                redirectStderr = true;
                appendStderr = true;
                errorFile = args[i + 1];

                args.erase(args.begin() + i, args.begin() + i + 2);
            }
            else
            {
                ++i;
            }
        }

        std::ofstream outFile;
        std::ostream* out = &std::cout;

        if (redirectStdout)
        {
            if (appendStdout)
                outFile.open(outputFile, std::ios::app);
            else
                outFile.open(outputFile);

            out = &outFile;
        }

        std::ofstream errFile;
        std::ostream* err = &std::cerr;

        if (redirectStderr)
        {
            if (appendStderr)
                errFile.open(errorFile, std::ios::app);
            else
                errFile.open(errorFile);

            err = &errFile;
        }

        if (args.empty())
            continue;

        if (args[0] == "exit")
        {
            break;
        }
        else if (args[0] == "echo")
        {
            for (size_t i = 1; i < args.size(); i++)
            {
                if (i > 1)
                    (*out) << " ";

                (*out) << args[i];
            }

            (*out) << std::endl;
        }
        else if (args[0] == "type")
        {
            if (args.size() < 2)
                continue;

            std::string cmd = args[1];

            if (cmd == "echo" ||
                cmd == "exit" ||
                cmd == "type" ||
                cmd == "pwd" ||
                cmd == "cd" ||
                cmd == "jobs")
            {
                (*out) << cmd << " is a shell builtin" << std::endl;
            }
            else
            {
                const char* pathEnv = std::getenv("PATH");
                std::string path = pathEnv ? pathEnv : "";

                std::stringstream ss(path);
                std::string dir;

                bool found = false;

                while (std::getline(ss, dir, ':'))
                {
                    std::string fullPath = dir + "/" + cmd;

                    if (access(fullPath.c_str(), X_OK) == 0)
                    {
                        (*out) << cmd << " is " << fullPath << std::endl;
                        found = true;
                        break;
                    }
                }

                if (!found)
                    (*err) << cmd << ": not found" << std::endl;
            }
        }
        else if (args[0] == "pwd")
        {
            char cwd[1024];

            if (getcwd(cwd, sizeof(cwd)) != nullptr)
                (*out) << cwd << std::endl;
        }
        else if (args[0] == "cd")
        {
            if (args.size() < 2)
                continue;

            std::string path = args[1];

            if (path == "~")
                path = getenv("HOME");

            if (chdir(path.c_str()) != 0)
            {
                (*err) << "cd: " << path
                       << ": No such file or directory"
                       << std::endl;
            }
        }
        else if (args[0] == "jobs")
        {
            // Empty implementation for this stage
        }
        else
        {
            std::vector<char*> argv;

            for (auto& arg : args)
                argv.push_back(const_cast<char*>(arg.c_str()));

            argv.push_back(nullptr);

            pid_t pid = fork();

            if (pid == 0)
            {
                if (redirectStdout)
                {
                    int fd;

                    if (appendStdout)
                    {
                        fd = open(
                            outputFile.c_str(),
                            O_WRONLY | O_CREAT | O_APPEND,
                            0644
                        );
                    }
                    else
                    {
                        fd = open(
                            outputFile.c_str(),
                            O_WRONLY | O_CREAT | O_TRUNC,
                            0644
                        );
                    }

                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                if (redirectStderr)
                {
                    int fd;

                    if (appendStderr)
                    {
                        fd = open(
                            errorFile.c_str(),
                            O_WRONLY | O_CREAT | O_APPEND,
                            0644
                        );
                    }
                    else
                    {
                        fd = open(
                            errorFile.c_str(),
                            O_WRONLY | O_CREAT | O_TRUNC,
                            0644
                        );
                    }

                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }

                execvp(argv[0], argv.data());

                std::cerr << args[0] << ": not found" << std::endl;
                exit(1);
            }

            if (is_background)
            {
                // Print job confirmation and track job count
                std::cout << "[" << job_count << "] " << pid << std::endl;
                job_count++;
                // DO NOT wait here so the prompt can return immediately
            }
            else
            {
                // Foreground command: wait for it to complete
                waitpid(pid, nullptr, 0);
            }
        }
    }

    return 0;
}