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
        std::uniform_real_distribution<double> is_free_rng{0, 1};
        std::uniform_real_distribution<double> pid_rng{1, (double)n_processes};
        std::normal_distribution<double> size_rng{40, 2};

        std::vector<size_t> last_element_offsets{n_processes, 0};

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
            elements[prev].next_offset = i + 1;
            last_element_offsets[pid] = i;

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
        const size_t print_limit = 20;

        std::cout << "ROW NUM | UID | ADDR | SIZE | PID | NEXT | PREV\n";
        for (int i = 0; i < std::min(print_limit, this->elements.size()); i++) {
            auto& el = this->elements[i];
            const char* sep = " | ";
            std::cout
                << i << sep 
                << el.uid << sep
                << std::hex << reinterpret_cast<uintptr_t>(el.data_ptr) << std::dec << sep
                << el.data_size << sep
                << el.pid << sep
                << el.next_offset << sep
                << el.prev_offset << '\n'; 
        }
        std::cout << std::flush;
    }
    ~List() {
        delete[] this->data;
    }
private:
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
