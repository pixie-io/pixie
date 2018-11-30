#include <iostream>
#include <string>

std::string Dog();

extern char _binary_throwaway_saying_txt_start;
extern char _binary_throwaway_saying_txt_end;

int main() {
  char* p = &_binary_throwaway_saying_txt_start;

  std::cout << "The saying is: "
            << std::string(p, 0,
                           _binary_throwaway_saying_txt_end - _binary_throwaway_saying_txt_start)
            << std::endl;
}
