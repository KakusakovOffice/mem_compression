#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <vector>
#include <random>
#include <limits.h>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
#include <cstring>
#include <iomanip>
#include <chrono>

class List {
public:
  class Element {
    public:
    uint64_t uid;        // Идентификатор элемента.
    uint8_t* data_ptr;   // Указатель на сегмент память.
    size_t data_size;    // Размер сегмента памяти.
    uint64_t pid;        // Номер процесса.
    size_t next_offset;  // Ссылка (индекс) на следующий элемент или SIZE_MAX если нет.
    size_t prev_offset;  // Ссылка (индекс) на предыдущий элемент или SIZE_MAX если нет.
  };

  class MemBlock {
    public:
    uint8_t* data_ptr;   // Указатель на память.
    size_t data_size;    // Размер памяти.
  };

  static List generate(size_t n_elements, double free_percent) {
    size_t n_processes = std::max(n_elements / 100, (size_t)3);

    std::random_device rd{};
    std::mt19937 rng{rd()};
    std::uniform_int_distribution<size_t> is_free_rng{0, 1};
    std::uniform_int_distribution<size_t> pid_rng{1, n_processes};
    std::normal_distribution<double> size_rng{40, 2};

    std::vector<size_t> last_element_offsets(n_processes + 1, SIZE_MAX);

    std::vector<Element> elements{};
    size_t total_size = 0;
    size_t free_size = 0;
    size_t uid_counter = 1;

    elements.reserve(n_elements);
    for (size_t i = 0; i < n_elements; i++) {
      size_t size = std::max((size_t)std::floor(size_rng(rng)), (size_t)1);

      uint64_t pid =
        is_free_rng(rng) < free_percent ?
          0 : (uint64_t)std::floor(pid_rng(rng));

      size_t prev = last_element_offsets[pid];

      if (prev != SIZE_MAX)
        elements[prev].next_offset = i;
      last_element_offsets[pid] = i;

      if (pid == 0)
        free_size += size;

      total_size += size;

      elements.push_back({
        .uid = uid_counter++,
        .data_ptr = nullptr,
        .data_size = size,
        .pid = pid,
        .next_offset = SIZE_MAX,
        .prev_offset = prev,
      });
    }

    uint8_t* data = new uint8_t[total_size]();
    uint8_t* data_ptr = data;
    for (auto& element : elements) {
      element.data_ptr = data_ptr;
      data_ptr += element.data_size;
    }

    return List{elements, data, free_size, total_size, uid_counter};
  }

  void defragment_optimized() {
    uint8_t* new_data = new uint8_t[total_size]();
    size_t offset = 0;

    for (auto& el : elements) {
      if (el.pid != 0) {
        std::memcpy(new_data + offset, el.data_ptr, el.data_size);
        el.data_ptr = new_data + offset;
        offset += el.data_size;
      }
    }

    std::unordered_map<size_t, size_t> old_to_new = get_link_mapping();
    fix_links(old_to_new);

    for (auto& block : blocks) {
      delete[] block.data_ptr;
    }

    blocks.clear();
    blocks.push_back({.data_ptr = new_data, .data_size = total_size});

    elements.push_back({
      .uid = uid_counter++,
      .data_ptr = new_data + offset,
      .data_size = free_size,
      .pid = 0,
      .next_offset = SIZE_MAX,
      .prev_offset = SIZE_MAX
    });

  }

  void defragment_slow() {
    size_t i = 0;
    for (size_t j = 0; j < elements.size(); j++) {
      if (elements[j].pid != 0) continue;

      i = j;
      break;
    }

    while (elements[i].prev_offset != SIZE_MAX) {
      i = elements[i].prev_offset;
    }

    uint8_t* occupied_block = new uint8_t[total_size - free_size]();
    size_t occupied_data_offset = 0;

    for (auto& el : elements) {
      if (el.pid != 0) {
        std::memcpy(occupied_block + occupied_data_offset, el.data_ptr, el.data_size);
        el.data_ptr = occupied_block + occupied_data_offset;
        occupied_data_offset += el.data_size;
      }
    }

    uint8_t* free_block = new uint8_t[free_size]();
    size_t new_data_offset = 0;

    do {
      std::memcpy(free_block + new_data_offset, elements[i].data_ptr, elements[i].data_size);
      new_data_offset += elements[i].data_size;

      i = elements[i].next_offset;
    } while (elements[i].next_offset != SIZE_MAX);

    for (auto& block : blocks) {
      delete[] block.data_ptr;
    }

    blocks.clear();
    blocks.push_back({.data_ptr = occupied_block, .data_size = occupied_data_offset});
    blocks.push_back({.data_ptr = free_block, .data_size = free_size});

    std::unordered_map<size_t, size_t> old_to_new = get_link_mapping();
    fix_links(old_to_new);

    elements.push_back({
      .uid = uid_counter++,
      .data_ptr = free_block,
      .data_size = free_size,
      .pid = 0,
      .next_offset = 0,
      .prev_offset = 0
    });
  }

  void print() const {
    const size_t n = elements.size();
    const size_t print_count = 100;

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

  List& operator=(const List& other) {
    if (this == &other) return *this;

    elements = other.elements;

    blocks.clear();

    for (const auto& block : other.blocks) {
      uint8_t* new_block = new uint8_t[block.data_size];
      std::memcpy(new_block, block.data_ptr, block.data_size);
      blocks.push_back({.data_ptr = new_block, .data_size = block.data_size});
    }

    for (auto& el : elements) {
      for (size_t i = 0; i < other.blocks.size(); i++) {
        uint8_t* old_block_start = other.blocks[i].data_ptr;
        uint8_t* old_block_end = old_block_start + other.blocks[i].data_size;

        if (el.data_ptr >= old_block_start && el.data_ptr < old_block_end) {
          size_t offset = el.data_ptr - old_block_start;
          el.data_ptr = blocks[i].data_ptr + offset;
          break;
        }
      }
    }

    free_size = other.free_size;
    total_size = other.total_size;
    uid_counter = other.uid_counter;
    return *this;
  }

  List(const List& other): elements(), blocks(), free_size(), total_size(0), uid_counter(0) {
    *this = other;
  }

  ~List() {
    for (auto& block : blocks) {
      delete[] block.data_ptr;
    }
  }

private:
  std::unordered_map<size_t, size_t> get_link_mapping() const {
    std::unordered_map<size_t, size_t> old_to_new;
    size_t new_index = 0;
    for (size_t i = 0; i < elements.size(); i++) {
      if (elements[i].pid != 0) {
        old_to_new[i] = new_index;
        new_index++;
      }
    }

    return old_to_new;
  }

  void fix_links(std::unordered_map<size_t, size_t>& old_to_new) {
    elements.erase(std::remove_if(elements.begin(), elements.end(), [&](Element& e) {
      if (e.pid == 0) {
        return true;
      } else {
        if (e.next_offset != SIZE_MAX) {
          e.next_offset = old_to_new[e.next_offset];
        }
        if (e.prev_offset != SIZE_MAX) {
          e.prev_offset = old_to_new[e.prev_offset];
        }
        return false;
      }
    }), elements.end());

    elements.shrink_to_fit();
  }

  void print_row(size_t index) const {
    const Element& elem = elements[index];

    std::cout << "| " << std::setw(6) << std::right << index << " ";
    std::cout << "| " << std::setw(16) << std::right << elem.uid << " ";
    std::cout << "|  0x" << std::setw(12) << std::right << std::hex << reinterpret_cast<uintptr_t>(elem.data_ptr) << std::dec << "  ";
    std::cout << "| " << std::setw(16) << std::right << elem.data_size << " ";
    std::cout << "| " << std::setw(16) << std::right << elem.pid << " ";

    if (elem.next_offset == SIZE_MAX) {
      std::cout << "| " << std::setw(16) << std::right << "N/A" << " ";
    } else {
      std::cout << "| " << std::setw(16) << std::right << elem.next_offset << " ";
    }

    if (elem.prev_offset == SIZE_MAX) {
      std::cout << "| " << std::setw(16) << std::right << "N/A" << " ";
    } else {
      std::cout << "| " << std::setw(16) << std::right << elem.prev_offset << " ";
    }

    std::cout << "|" << std::endl;
  }

  List(
    std::vector<Element> elements,
    uint8_t* data,
    size_t free_size,
    size_t total_size,
    size_t uid_counter
  ):
  elements(elements),
  blocks{{.data_ptr = data, .data_size = total_size}},
  free_size(free_size),
  total_size(total_size),
  uid_counter(uid_counter) {}

  std::vector<MemBlock> blocks;
  std::vector<Element> elements;
  size_t free_size;
  size_t total_size;
  size_t uid_counter;
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
    ss.clear();
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

  auto list1 = List::generate(n_rows, (double)free_percent/100);
  auto list2 = list1;
  list1.print();

  auto start = std::chrono::steady_clock::now();
  list1.defragment_optimized();
  auto end = std::chrono::steady_clock::now();
  std::cout << "Время выполнения defragment_optimized: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

  list1.print();

  start = std::chrono::steady_clock::now();
  list2.defragment_slow();
  end = std::chrono::steady_clock::now();
  std::cout << "Время выполнения defragment_slow: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

  list2.print();

  return 0;
}
