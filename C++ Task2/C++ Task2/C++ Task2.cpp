#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <ctime>
#include <algorithm>
#include <memory>

using namespace std;

// Интерфейс типа читателя
class IReaderType {
public:
    virtual ~IReaderType() = default;
    virtual int loseChance() const = 0;
    virtual int lateChance() const = 0;
    virtual int maxLateDays() const = 0;
    virtual string getName() const = 0;
    virtual unique_ptr<IReaderType> clone() const = 0;
};

// Конкретные реализации типов читателей
class OrdinaryReader : public IReaderType {
public:
    int loseChance() const override { return 5; }
    int lateChance() const override { return 0; }
    int maxLateDays() const override { return 0; }
    string getName() const override { return "Обычный"; }
    unique_ptr<IReaderType> clone() const override { return make_unique<OrdinaryReader>(*this); }
};

class GreedyReader : public IReaderType {
public:
    int loseChance() const override { return 10; }
    int lateChance() const override { return 5; }
    int maxLateDays() const override { return 0; }
    string getName() const override { return "Жадный"; }
    unique_ptr<IReaderType> clone() const override { return make_unique<GreedyReader>(*this); }
};

class ForgetfulReader : public IReaderType {
public:
    int loseChance() const override { return 5; }
    int lateChance() const override { return 30; }
    int maxLateDays() const override { return 3; }
    string getName() const override { return "Забывчивый"; }
    unique_ptr<IReaderType> clone() const override { return make_unique<ForgetfulReader>(*this); }
};

struct Book {
    string title;
    int returnDay = 0;  // день, до которого книгу надо вернуть
    bool isLost = false;
    bool isReturnedLate = false;
    bool isTaken = false; // Флаг, указывающий что книга выдана
};

struct Reader {
    unique_ptr<IReaderType> type;
    string name;
    vector<Book> takenBooks;

    // Конструктор
    Reader(unique_ptr<IReaderType> t, string n, vector<Book> books = {})
        : type(std::move(t)), name(std::move(n)), takenBooks(std::move(books)) {}

    // Конструктор перемещения
    Reader(Reader&& other) noexcept
        : type(std::move(other.type)), name(std::move(other.name)), takenBooks(std::move(other.takenBooks)) {}

    // Оператор присваивания перемещением
    Reader& operator=(Reader&& other) noexcept {
        if (this != &other) {
            type = std::move(other.type);
            name = std::move(other.name);
            takenBooks = std::move(other.takenBooks);
        }
        return *this;
    }

    // Запрещаем копирование
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    int loseChance() const { return type->loseChance(); }
    int lateChance() const { return type->lateChance(); }
    int maxLateDays() const { return type->maxLateDays(); }
    string getTypeName() const { return type->getName(); }
};

vector<Book> libraryBooks;
vector<Reader> activeReaders;
vector<Book> lostBooks;
vector<Book> lateReturnedBooks;

mt19937 rng(static_cast<unsigned int>(time(0)));
uniform_int_distribution<int> dist100(1, 100);
uniform_int_distribution<int> returnDays(6, 10); // срок возврата 6-10 дней
uniform_int_distribution<int> lateDays(1, 3); // дни опоздания для забывчивых

void initLibrary() {
    libraryBooks = {
        {"Преступление и наказание"},
        {"Чистый код"},
        {"Война и мир"},
        {"1984"},
        {"Мастер и Маргарита"}
    };
}

bool giveBookToReader(Reader& reader, Book& book, int currentDay) {
    if (book.isTaken) return false; // Книга уже выдана

    int chance = dist100(rng);

    book.isLost = (chance <= reader.loseChance());
    if (!book.isLost) {
        book.isReturnedLate = (chance <= reader.loseChance() + reader.lateChance());
    }

    book.returnDay = currentDay + returnDays(rng);
    if (book.isReturnedLate && reader.maxLateDays() > 0) {
        book.returnDay += lateDays(rng);
    }

    book.isTaken = true;
    reader.takenBooks.push_back(book);
    return true;
}

Reader createRandomReader(int day) {
    uniform_int_distribution<int> typeDist(0, 2);
    int type = typeDist(rng);

    unique_ptr<IReaderType> readerType;
    switch (type) {
    case 0: readerType = make_unique<OrdinaryReader>(); break;
    case 1: readerType = make_unique<GreedyReader>(); break;
    case 2: readerType = make_unique<ForgetfulReader>(); break;
    }

    string readerName = readerType->getName() + "_" + to_string(day);
    return Reader(std::move(readerType), readerName);
}

void returnBookToLibrary(Book& book) {
    book.isTaken = false;
    // Сбрасываем флаги, кроме isLost (потерянные книги не возвращаются)
    if (!book.isLost) {
        book.isReturnedLate = false;
    }
}

void processDay(int day) {
    cout << "=== День " << day << " ===\n\n";

    // Добавление нового читателя
    if (dist100(rng) <= 50) {
        // Получаем список доступных книг
        vector<size_t> availableIndices;
        for (size_t i = 0; i < libraryBooks.size(); ++i) {
            if (!libraryBooks[i].isTaken) {
                availableIndices.push_back(i);
            }
        }

        if (!availableIndices.empty()) {
            uniform_int_distribution<size_t> bookDist(0, availableIndices.size() - 1);
            size_t bookIndex = availableIndices[bookDist(rng)];
            Reader newReader = createRandomReader(day);

            if (giveBookToReader(newReader, libraryBooks[bookIndex], day)) {
                activeReaders.push_back(std::move(newReader));
            }
        }
    }

    // Обработка возвратов
    for (auto it = activeReaders.begin(); it != activeReaders.end(); ) {
        bool readerRemoved = false;

        for (auto bookIt = it->takenBooks.begin(); bookIt != it->takenBooks.end(); ) {
            if (day > bookIt->returnDay) {
                if (bookIt->isLost) {
                    // Книга считается потерянной только на следующий день после крайнего срока
                    if (day == bookIt->returnDay + 1) {
                        lostBooks.push_back(*bookIt);
                        cout << "Книга " << bookIt->title << " потеряна читателем " << it->name << endl;
                    }
                    bookIt = it->takenBooks.erase(bookIt);
                }
                else {
                    if (bookIt->isReturnedLate) {
                        lateReturnedBooks.push_back(*bookIt);
                    }
                    // Возвращаем книгу в библиотеку
                    for (auto& libBook : libraryBooks) {
                        if (libBook.title == bookIt->title) {
                            returnBookToLibrary(libBook);
                            break;
                        }
                    }
                    bookIt = it->takenBooks.erase(bookIt);
                }
            }
            else {
                ++bookIt;
            }
        }

        if (it->takenBooks.empty()) {
            it = activeReaders.erase(it);
            readerRemoved = true;
        }

        if (!readerRemoved) {
            ++it;
        }
    }

    // Вывод информации
    cout << "Доступные книги:\n";
    bool hasAvailable = false;
    for (const auto& book : libraryBooks) {
        if (!book.isTaken) {
            cout << " - " << book.title << "\n";
            hasAvailable = true;
        }
    }
    if (!hasAvailable) cout << " - Нет доступных книг\n";

    cout << "\nАктивные читатели:\n";
    if (activeReaders.empty()) cout << " - Нет активных читателей\n";
    else for (const auto& reader : activeReaders) {
        cout << " - " << reader.name << " взял:\n";
        for (const auto& book : reader.takenBooks) {
            cout << "     * " << book.title << " (до дня " << book.returnDay << ")";
            if (book.isLost) cout << " - БУДЕТ ПОТЕРЯНА";
            else if (book.isReturnedLate) cout << " - БУДЕТ ОПОЗДАНИЕ";
            cout << "\n";
        }
    }

    cout << "\nПотерянные книги:\n";
    if (lostBooks.empty()) cout << " - Нет потерянных книг\n";
    else for (const auto& book : lostBooks) cout << " - " << book.title << "\n";

    cout << "\nВозвращённые с опозданием книги (за все время):\n";
    if (lateReturnedBooks.empty()) cout << " - Нет возвращённых с опозданием\n";
    else for (const auto& book : lateReturnedBooks) cout << " - " << book.title << "\n";

    cout << "\n";
}

int main() {
    setlocale(LC_ALL, "Russian");
    initLibrary();

    int totalDays = 50;
    for (int day = 1; day <= totalDays; ++day) {
        processDay(day);
    }

    return 0;
}