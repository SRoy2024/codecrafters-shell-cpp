#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>

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
      if((command.substr(5) == "echo" || command.substr(5) == "exit" || command.substr(5) == "type"))
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

            std::ifstream file(fullPath);

            if(file.good())
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
    else
      std::cout<<command<<": not found"<<std::endl;
  }
}
