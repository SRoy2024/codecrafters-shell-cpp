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
    else if (command == "input")
      break;
    std::cout<<command<<": command not found"<<std::endl;
  }
}
