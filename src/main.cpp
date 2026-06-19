#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <sys/wait.h>
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while(true)
  {
    std::cout << "$ ";
    std::string command;
    if(!std::getline(std::cin, command))
      break;
    else if (command == "exit")
      break;
    if (command.substr(0,5) == "echo ")
    {
      std::cout<<command.substr(5)<<std::endl;
    }
    else if(command.substr(0,5) == "type " )
    {
      if((command.substr(5) == "echo" || command.substr(5) == "exit" || command.substr(5) == "type" || command.substr(5) == "pwd" || command.substr(5) == "cd"))
        std::cout<<command.substr(5)<<" is a shell builtin"<<std::endl;
      else
      {
        std::string cmd = command.substr(5);
        std::string path = std::getenv("PATH");
        std::stringstream ss(path);
        std::string dir;

        bool found = false;

        while(std::getline(ss, dir, ':'))
        {
            std::string fullPath = dir + "/" + cmd;

            if(access(fullPath.c_str(), X_OK) == 0)
            {
                std::cout << cmd << " is " << fullPath << std::endl;
                found = true;
                break;
            }
        }

        if(!found)
            std::cout << cmd << ": not found" << std::endl;
      }
    }
    else if ( command == "pwd")
    {
      char cwd[1024];
      if(getcwd(cwd, sizeof(cwd)) != nullptr)
          std::cout<<cwd<<std::endl;
    }
    else if ( command.rfind("cd", 0) == 0)
    {
      std::string Path = command.substr(3);
      if(chdir(Path.c_str()) != 0)
        std::cout<<"cd::"<<Path<<": No such file or directory"<<std::endl;
    }
    else
    {
        std::stringstream ss(command);
        std::vector<std::string> args;
        std::string token;

        while (ss >> token)
            args.push_back(token);

        std::vector<char*> argv;
        for (auto& arg : args)
            argv.push_back(const_cast<char*>(arg.c_str()));

        argv.push_back(nullptr);

        pid_t pid = fork();

        if (pid == 0)
        {
            execvp(argv[0], argv.data());

            std::cout << args[0] << ": not found" << std::endl;
            exit(1);
        }

        wait(nullptr);
    }
  }
}
