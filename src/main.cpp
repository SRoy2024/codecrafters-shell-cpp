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

        // Handle stdout redirection (> and 1>)
        std::string outputFile;
        bool redirectStdout = false;

        for (size_t i = 0; i + 1 < args.size(); i++)
        {
            if (args[i] == ">" || args[i] == "1>")
            {
                redirectStdout = true;
                outputFile = args[i + 1];

                args.erase(args.begin() + i, args.begin() + i + 2);
                break;
            }
        }

        std::ofstream file;
        std::ostream* out = &std::cout;

        if (redirectStdout)
        {
            file.open(outputFile);
            out = &file;
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
                cmd == "pwd"  ||
                cmd == "cd")
            {
                (*out) << cmd << " is a shell builtin" << std::endl;
            }
            else
            {
                std::string path = std::getenv("PATH");
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
                    (*out) << cmd << ": not found" << std::endl;
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
                std::cout << "cd: "
                          << path
                          << ": No such file or directory"
                          << std::endl;
            }
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
                    int fd = open(
                        outputFile.c_str(),
                        O_WRONLY | O_CREAT | O_TRUNC,
                        0644
                    );

                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                execvp(argv[0], argv.data());

                std::cout << args[0] << ": not found" << std::endl;
                exit(1);
            }

            wait(nullptr);
        }
    }

    return 0;
}