#include <iostream>
#include <sstream>
#include <stdint.h>
#include <vector>
#include <random>
#include <limits.h>
#include <cstdlib>
#include <algorithm>
#include <iomanip>

// Парсим аргументы
// list1 = init_list()
// list2 = copy_list(list1)
// print_list(list1)
// compress_list(list1)
// print_list(list1)
// compress_list(list2)
// print_list(list2)
// log_list() // maybe?

class List {
  public:
  class Element {
    public:
    uint64_t uid;  // Идентификатор элемента.
    uint8_t* data_ptr;  // Указатель на сегмент память.
    size_t data_size;  // Размер сегмента памяти.
    uint64_t pid;  // Номер процесса.
    size_t next_offset;  // Ссылка (индекс + 1) на следующий элемент того же процесса.
    size_t prev_offset;  // Ссылка (индекс + 1) на предыдущий элемент того же процесса.
  };

  static List generate(size_t n_elements, double free_percent) {
    size_t n_processes = std::max(n_elements / 100, (size_t)3);

    std::random_device rd{};
    std::mt19937 rng{rd()};
    std::uniform_int_distribution<size_t> is_free_rng{0, 1};
    std::uniform_int_distribution<size_t> pid_rng{1, n_processes};
    std::normal_distribution<double> size_rng{40, 2};

    std::vector<size_t> last_element_offsets(n_processes + 1, 0);

    std::vector<Element> elements{};
    size_t total_size = 0;
    elements.reserve(n_elements);
    for (size_t i = 0; i < n_elements; i++) {
      uint64_t uid = i + 1;
      size_t size = std::max((size_t)std::floor(size_rng(rng)), (size_t)1);
      total_size += size;
      uint64_t pid = 0;
      bool is_free = is_free_rng(rng) < free_percent;
      if (!is_free) {
        pid = (uint64_t)std::floor(pid_rng(rng));
      }
      size_t prev = last_element_offsets[pid];
      if (prev != 0)
        elements[prev - 1].next_offset = i + 1;
      last_element_offsets[pid] = i + 1;


      Element element{
        .uid = uid,
        .data_ptr = nullptr,
        .data_size = size,
        .pid = pid,
        .next_offset = 0,
        .prev_offset = prev,
      };
      elements.push_back(element);
    }

    uint8_t* data = new uint8_t[total_size]();
    uint8_t* data_ptr = data;
    for (auto& element : elements) {
      element.data_ptr = data_ptr;
      data_ptr += element.data_size;
    }

    return List{elements, data};
  }

  void print() {
    const size_t n = elements.size();
    const size_t print_count = 10;

    std::cout << "+--------+------------------+------------------+------------------+------------------+------------------+------------------+" << std::endl;
    std::cout << "| Row No.|       UID        |     Address      |       Size       |     Process      |       Next       |     Previous     |" << std::endl;
    std::cout << "+--------+------------------+------------------+------------------+------------------+------------------+------------------+" << std::endl;

    size_t first_limit = std::min(print_count, n);
    for (size_t i = 0; i < first_limit; i++) {
      print_row(i);
    }

    if (n > 2 * print_count) {
      std::cout << "|  ...   |       ...        |       ...        |       ...        |       ...        |       ...        |       ...        |" << std::endl;
    }

    if (n > print_count) {
      size_t last_start = std::max(first_limit, n - print_count);
      for (size_t i = last_start; i < n; i++) {
        print_row(i);
      }
    }

    std::cout << "+--------+------------------+------------------+------------------+------------------+------------------+------------------+" << std::endl;
    std::cout << "Total elements: " << n << std::endl;
  }

  ~List() {
    delete[] this->data;
  }
  private:
  void print_row(size_t index) {
    const Element& elem = elements[index];

    std::cout << "| " << std::setw(6) << std::right << index << " ";
    std::cout << "| " << std::setw(16) << std::right << elem.uid << " ";
    std::cout << "|  0x" << std::setw(12) << std::right << std::hex << reinterpret_cast<uintptr_t>(elem.data_ptr) << std::dec << "  ";
    std::cout << "| " << std::setw(16) << std::right << elem.data_size << " ";
    std::cout << "| " << std::setw(16) << std::right << elem.pid << " ";

    if (elem.next_offset == 0) {
      std::cout << "| " << std::setw(16) << std::right << "N/A" << " ";
    } else {
      std::cout << "| " << std::setw(16) << std::right << (elem.next_offset - 1) << " ";
    }

    if (elem.prev_offset == 0) {
      std::cout << "| " << std::setw(16) << std::right << "N/A" << " ";
    } else {
      std::cout << "| " << std::setw(16) << std::right << (elem.prev_offset - 1) << " ";
    }

    std::cout << "|" << std::endl;
  }

  List(std::vector<Element> elements, uint8_t* data) {
    this->elements = elements;
    this->data = data;
  }
  std::vector<Element> elements;
  uint8_t* data;
};

int main(int argc, char *argv[]) {
  std::stringstream ss;
  uint64_t n_rows = 100;
  if (argc >= 2) {
    ss << argv[1];
    ss >> n_rows;
    if (ss.fail()) {
      std::cerr
      <<  "Первый аргумент программы введен неправильно -"
      " он должен содержать число сторк в таблице."
      << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }
  uint64_t free_percent = 50;
  if (argc >= 3) {
    ss << argv[2];
    ss >> free_percent;
    if (ss.fail()) {
      std::cerr
      <<  "Второй аргумент программы введен неправильно -"
      " он должен содержать процент свободных строк (от 0 до 100, целое число)."
      << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }

  const uint64_t n_rows_limit = (uint64_t)std::numeric_limits<size_t>::max();
  if (n_rows > n_rows_limit) {
    std::cerr
    << "Слишком большое количество строк в таблице (первый аргумент), максимум: "
    << n_rows_limit
    << std::endl;
    std::exit(EXIT_FAILURE);
  }

  const uint64_t free_precent_limit = 100;
  if (free_percent > free_precent_limit) {
    std::cerr
    << "Слишком большой процент пустых строк в таблице (второй аргумент), максимум: "
    << std::endl;
    std::exit(EXIT_FAILURE);
  }

  auto list = List::generate(n_rows, (double)free_percent/100);
  list.print();

  return 0;
}
