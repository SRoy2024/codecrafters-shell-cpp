#include <iostream>
#include <string>

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
    if (command.substr(0,5) == "echo")
    {
      std::cout<<command.substr(5)<<std::endl;
    }
    else
      std::cout<<command<<": command not found"<<std::endl;
  }
}
