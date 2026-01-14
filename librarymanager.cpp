// librarymanager.cpp
#include "librarymanager.h"
#include <QtWidgets>
#include <QtSql>
#include <QMessageBox>
#include <QDateTime>
#include <QFileDialog>
#include <QPrinter>
#include <QPrintDialog>
#include <QDesktopServices>
#include <QMessageBox>
void LibraryManager::setupUI()
{
    // 设置主窗口
    setMinimumSize(1200, 700);

    // 创建标签页
    tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // 创建标签页内容
    createBookManagementTab();
    createReaderManagementTab();
    createBorrowReturnTab();
    createStatisticsTab();
}

void LibraryManager::createBookManagementTab()
{
    QWidget *bookTab = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(bookTab);

    // 搜索区域
    QGroupBox *searchGroup = new QGroupBox("图书搜索");
    QGridLayout *searchLayout = new QGridLayout;

    searchLayout->addWidget(new QLabel("图书ID:"), 0, 0);
    bookIdFilter = new QLineEdit;
    searchLayout->addWidget(bookIdFilter, 0, 1);

    searchLayout->addWidget(new QLabel("书名:"), 0, 2);
    bookTitleFilter = new QLineEdit;
    searchLayout->addWidget(bookTitleFilter, 0, 3);

    searchLayout->addWidget(new QLabel("作者:"), 1, 0);
    bookAuthorFilter = new QLineEdit;
    searchLayout->addWidget(bookAuthorFilter, 1, 1);

    searchLayout->addWidget(new QLabel("ISBN:"), 1, 2);
    bookIsbnFilter = new QLineEdit;
    searchLayout->addWidget(bookIsbnFilter, 1, 3);

    searchLayout->addWidget(new QLabel("分类:"), 2, 0);
    bookCategoryFilter = new QComboBox;
    bookCategoryFilter->setEditable(true);
    bookCategoryFilter->addItem("所有分类");
    bookCategoryFilter->addItems({"编程", "文学", "科学", "历史", "艺术", "教育"});
    searchLayout->addWidget(bookCategoryFilter, 2, 1);

    searchLayout->addWidget(new QLabel("状态:"), 2, 2);
    bookStatusFilter = new QComboBox;
    bookStatusFilter->addItem("所有状态");
    bookStatusFilter->addItems({"在库", "借出", "维护中"});
    searchLayout->addWidget(bookStatusFilter, 2, 3);

    QPushButton *searchButton = new QPushButton("搜索");
    connect(searchButton, &QPushButton::clicked, this, &LibraryManager::searchBooks);
    searchLayout->addWidget(searchButton, 3, 2);

    QPushButton *clearButton = new QPushButton("清除");
    connect(clearButton, &QPushButton::clicked, this, &LibraryManager::clearBookSearch);
    searchLayout->addWidget(clearButton, 3, 3);

    searchGroup->setLayout(searchLayout);
    mainLayout->addWidget(searchGroup);

    // 图书表格
    bookTableView = new QTableView;
    bookModel = new QSqlTableModel(this, db);
    bookModel->setTable("books");
    bookModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    bookModel->select();

    // 设置表头
    bookModel->setHeaderData(0, Qt::Horizontal, "ID");
    bookModel->setHeaderData(1, Qt::Horizontal, "ISBN");
    bookModel->setHeaderData(2, Qt::Horizontal, "书名");
    bookModel->setHeaderData(3, Qt::Horizontal, "作者");
    bookModel->setHeaderData(4, Qt::Horizontal, "出版社");
    bookModel->setHeaderData(5, Qt::Horizontal, "出版日期");
    bookModel->setHeaderData(6, Qt::Horizontal, "分类");
    bookModel->setHeaderData(7, Qt::Horizontal, "价格");
    bookModel->setHeaderData(8, Qt::Horizontal, "总数量");
    bookModel->setHeaderData(9, Qt::Horizontal, "可借数量");
    bookModel->setHeaderData(10, Qt::Horizontal, "位置");
    bookModel->setHeaderData(11, Qt::Horizontal, "状态");

    bookTableView->setModel(bookModel);
    bookTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    bookTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    bookTableView->resizeColumnsToContents();

    mainLayout->addWidget(bookTableView);

    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout;

    QPushButton *addButton = new QPushButton("添加图书");
    connect(addButton, &QPushButton::clicked, this, &LibraryManager::addBook);
    buttonLayout->addWidget(addButton);

    QPushButton *editButton = new QPushButton("编辑图书");
    connect(editButton, &QPushButton::clicked, this, &LibraryManager::editBook);
    buttonLayout->addWidget(editButton);

    QPushButton *deleteButton = new QPushButton("删除图书");
    connect(deleteButton, &QPushButton::clicked, this, &LibraryManager::deleteBook);
    buttonLayout->addWidget(deleteButton);

    QPushButton *refreshButton = new QPushButton("刷新");
    connect(refreshButton, &QPushButton::clicked, [this]() { bookModel->select(); });
    buttonLayout->addWidget(refreshButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    tabWidget->addTab(bookTab, "图书管理");
}

void LibraryManager::createReaderManagementTab()
{
    QWidget *readerTab = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(readerTab);

    // 搜索区域
    QGroupBox *searchGroup = new QGroupBox("读者搜索");
    QGridLayout *searchLayout = new QGridLayout;

    searchLayout->addWidget(new QLabel("读者ID:"), 0, 0);
    readerIdFilter = new QLineEdit;
    searchLayout->addWidget(readerIdFilter, 0, 1);

    searchLayout->addWidget(new QLabel("姓名:"), 0, 2);
    readerNameFilter = new QLineEdit;
    searchLayout->addWidget(readerNameFilter, 0, 3);

    searchLayout->addWidget(new QLabel("电话:"), 1, 0);
    readerPhoneFilter = new QLineEdit;
    searchLayout->addWidget(readerPhoneFilter, 1, 1);

    searchLayout->addWidget(new QLabel("类型:"), 1, 2);
    readerTypeFilter = new QComboBox;
    readerTypeFilter->setEditable(true);
    readerTypeFilter->addItem("所有类型");
    readerTypeFilter->addItems({"普通读者", "学生", "教师", "VIP"});
    searchLayout->addWidget(readerTypeFilter, 1, 3);

    QPushButton *searchButton = new QPushButton("搜索");
    connect(searchButton, &QPushButton::clicked, this, &LibraryManager::searchReaders);
    searchLayout->addWidget(searchButton, 2, 2);

    QPushButton *clearButton = new QPushButton("清除");
    connect(clearButton, &QPushButton::clicked, this, &LibraryManager::clearReaderSearch);
    searchLayout->addWidget(clearButton, 2, 3);

    searchGroup->setLayout(searchLayout);
    mainLayout->addWidget(searchGroup);

    // 读者表格
    readerTableView = new QTableView;
    readerModel = new QSqlTableModel(this, db);
    readerModel->setTable("readers");
    readerModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    readerModel->select();

    // 设置表头
    readerModel->setHeaderData(0, Qt::Horizontal, "ID");
    readerModel->setHeaderData(1, Qt::Horizontal, "借书证号");
    readerModel->setHeaderData(2, Qt::Horizontal, "姓名");
    readerModel->setHeaderData(3, Qt::Horizontal, "性别");
    readerModel->setHeaderData(4, Qt::Horizontal, "电话");
    readerModel->setHeaderData(5, Qt::Horizontal, "邮箱");
    readerModel->setHeaderData(6, Qt::Horizontal, "类型");
    readerModel->setHeaderData(7, Qt::Horizontal, "状态");

    readerTableView->setModel(readerModel);
    readerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    readerTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    readerTableView->resizeColumnsToContents();

    mainLayout->addWidget(readerTableView);

    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout;

    QPushButton *addButton = new QPushButton("添加读者");
    connect(addButton, &QPushButton::clicked, this, &LibraryManager::addReader);
    buttonLayout->addWidget(addButton);

    QPushButton *editButton = new QPushButton("编辑读者");
    connect(editButton, &QPushButton::clicked, this, &LibraryManager::editReader);
    buttonLayout->addWidget(editButton);

    QPushButton *deleteButton = new QPushButton("删除读者");
    connect(deleteButton, &QPushButton::clicked, this, &LibraryManager::deleteReader);
    buttonLayout->addWidget(deleteButton);

    QPushButton *refreshButton = new QPushButton("刷新");
    connect(refreshButton, &QPushButton::clicked, [this]() { readerModel->select(); });
    buttonLayout->addWidget(refreshButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    tabWidget->addTab(readerTab, "读者管理");
}

void LibraryManager::createBorrowReturnTab()
{
    QWidget *borrowTab = new QWidget;
    QHBoxLayout *mainLayout = new QHBoxLayout(borrowTab);

    // 左侧：借还书操作
    QWidget *operationWidget = new QWidget;
    QVBoxLayout *operationLayout = new QVBoxLayout(operationWidget);

    // 借书区域
    QGroupBox *borrowGroup = new QGroupBox("借书");
    QFormLayout *borrowLayout = new QFormLayout;

    borrowBookId = new QLineEdit;
    borrowLayout->addRow("图书ID:", borrowBookId);

    borrowReaderId = new QLineEdit;
    borrowLayout->addRow("读者ID:", borrowReaderId);

    borrowDays = new QSpinBox;
    borrowDays->setRange(1, 180);
    borrowDays->setValue(30);
    borrowLayout->addRow("借阅天数:", borrowDays);

    QPushButton *borrowButton = new QPushButton("借书");
    connect(borrowButton, &QPushButton::clicked, this, &LibraryManager::borrowBook);
    borrowLayout->addRow(borrowButton);

    borrowGroup->setLayout(borrowLayout);
    operationLayout->addWidget(borrowGroup);

    // 还书区域
    QGroupBox *returnGroup = new QGroupBox("还书");
    QFormLayout *returnLayout = new QFormLayout;

    returnRecordId = new QLineEdit;
    returnLayout->addRow("借阅记录ID:", returnRecordId);

    QPushButton *returnButton = new QPushButton("还书");
    connect(returnButton, &QPushButton::clicked, this, &LibraryManager::returnBook);
    returnLayout->addRow(returnButton);

    QPushButton *renewButton = new QPushButton("续借");
    connect(renewButton, &QPushButton::clicked, this, &LibraryManager::renewBook);
    returnLayout->addRow(renewButton);

    returnGroup->setLayout(returnLayout);
    operationLayout->addWidget(returnGroup);

    // 逾期书籍查看
    QPushButton *overdueButton = new QPushButton("查看逾期书籍");
    connect(overdueButton, &QPushButton::clicked, this, &LibraryManager::showOverdueList);
    operationLayout->addWidget(overdueButton);

    operationLayout->addStretch();
    mainLayout->addWidget(operationWidget, 1);

    // 右侧：借阅记录表格
    QWidget *recordWidget = new QWidget;
    QVBoxLayout *recordLayout = new QVBoxLayout(recordWidget);

    QLabel *recordLabel = new QLabel("借阅记录");
    recordLabel->setAlignment(Qt::AlignCenter);
    QFont font = recordLabel->font();
    font.setPointSize(12);
    font.setBold(true);
    recordLabel->setFont(font);
    recordLayout->addWidget(recordLabel);

    borrowTableView = new QTableView;
    borrowModel = new QSqlTableModel(this, db);
    borrowModel->setTable("borrow_records");
    borrowModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    borrowModel->setFilter("status = '借出'");
    borrowModel->select();

    // 设置表头
    borrowModel->setHeaderData(0, Qt::Horizontal, "记录ID");
    borrowModel->setHeaderData(1, Qt::Horizontal, "图书ID");
    borrowModel->setHeaderData(2, Qt::Horizontal, "读者ID");
    borrowModel->setHeaderData(3, Qt::Horizontal, "借书日期");
    borrowModel->setHeaderData(4, Qt::Horizontal, "应还日期");
    borrowModel->setHeaderData(5, Qt::Horizontal, "状态");

    borrowTableView->setModel(borrowModel);
    borrowTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    borrowTableView->resizeColumnsToContents();

    recordLayout->addWidget(borrowTableView);
    mainLayout->addWidget(recordWidget, 2);

    tabWidget->addTab(borrowTab, "借还书管理");
}