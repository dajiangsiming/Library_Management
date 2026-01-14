// librarymanager.h
#ifndef LIBRARYMANAGER_H
#define LIBRARYMANAGER_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QStandardItemModel>
#include <QTimer>
#include <QSystemTrayIcon>

class QTabWidget;
class QTableView;
class QLineEdit;
class QComboBox;
class QPushButton;
class QDateEdit;
class QLabel;
class QTextEdit;
class QGroupBox;
class QSpinBox;
class QCheckBox;

class LibraryManager : public QMainWindow
{
    Q_OBJECT

public:
    LibraryManager(QWidget *parent = nullptr);
    ~LibraryManager();

private slots:
    // 图书管理
    void addBook();
    void editBook();
    void deleteBook();
    void searchBooks();
    void clearBookSearch();

    // 读者管理
    void addReader();
    void editReader();
    void deleteReader();
    void searchReaders();
    void clearReaderSearch();

    // 借还书管理
    void borrowBook();
    void returnBook();
    void renewBook();

    // 统计
    void refreshStatistics();
    void generateReport();

    // 逾期提醒
    void checkOverdueBooks();
    void showOverdueList();

    // 系统
    void setupDatabase();
    void backupDatabase();
    void restoreDatabase();
    void about();

    void createBookManagementTab();
    void createReaderManagementTab();
    void createBorrowReturnTab();
    void createStatisticsTab();

    QIcon createIcon(const QString &color, const QString &symbol);

private:
    void setupUI();
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void createModels();
    void applyFilters();

    // UI组件
    QTabWidget *tabWidget;

    // 图书管理页
    QTableView *bookTableView;
    QSqlTableModel *bookModel;
    QLineEdit *bookIdFilter;
    QLineEdit *bookTitleFilter;
    QLineEdit *bookAuthorFilter;
    QLineEdit *bookIsbnFilter;
    QComboBox *bookCategoryFilter;
    QComboBox *bookStatusFilter;

    // 读者管理页
    QTableView *readerTableView;
    QSqlTableModel *readerModel;
    QLineEdit *readerIdFilter;
    QLineEdit *readerNameFilter;
    QLineEdit *readerPhoneFilter;
    QComboBox *readerTypeFilter;

    // 借阅记录页
    QTableView *borrowTableView;
    QSqlTableModel *borrowModel;

    // 借还书操作
    QLineEdit *borrowBookId;
    QLineEdit *borrowReaderId;
    QSpinBox *borrowDays;
    QLineEdit *returnRecordId;

    // 统计页
    QLabel *totalBooksLabel;
    QLabel *totalReadersLabel;
    QLabel *borrowedBooksLabel;
    QLabel *overdueBooksLabel;
    QLabel *popularCategoryLabel;
    QLabel *activeReadersLabel;
    QTextEdit *reportTextEdit;

    // 数据库
    QSqlDatabase db;

    // 定时器用于逾期检查
    QTimer *overdueTimer;
    QSystemTrayIcon *trayIcon;

    // 模型
    QStandardItemModel *statisticsModel;
};

#endif // LIBRARYMANAGER_H
