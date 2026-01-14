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


LibraryManager::LibraryManager(QWidget *parent)
    : QMainWindow(parent)
    , overdueTimer(new QTimer(this))
    , trayIcon(new QSystemTrayIcon(this))
{
    setupDatabase();
    setupUI();
    createMenuBar();
    createToolBar();
    createStatusBar();
    createModels();

    // 设置定时器检查逾期书籍（每小时检查一次）
    connect(overdueTimer, &QTimer::timeout, this, &LibraryManager::checkOverdueBooks);
    overdueTimer->start(3600000); // 1小时

    // 启动时立即检查一次
    checkOverdueBooks();

    // 设置系统托盘
    trayIcon->setIcon(QIcon(":/icons/library.png"));
    trayIcon->setToolTip("图书馆管理系统");
    trayIcon->show();
}

LibraryManager::~LibraryManager()
{
    if (db.isOpen()) {
        db.close();
    }
}

void LibraryManager::setupDatabase()
{
    // 连接SQLite数据库
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("library.db");

    if (!db.open()) {
        QMessageBox::critical(this, "错误", "无法打开数据库！");
        return;
    }

    QSqlQuery query;

    // 创建图书表
    query.exec("CREATE TABLE IF NOT EXISTS books ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "isbn TEXT UNIQUE NOT NULL,"
               "title TEXT NOT NULL,"
               "author TEXT NOT NULL,"
               "publisher TEXT,"
               "publish_date DATE,"
               "category TEXT,"
               "price REAL,"
               "total_copies INTEGER DEFAULT 1,"
               "available_copies INTEGER DEFAULT 1,"
               "location TEXT,"
               "description TEXT,"
               "status TEXT DEFAULT '在库',"
               "created_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");

    // 创建读者表
    query.exec("CREATE TABLE IF NOT EXISTS readers ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "card_number TEXT UNIQUE NOT NULL,"
               "name TEXT NOT NULL,"
               "gender TEXT,"
               "birth_date DATE,"
               "phone TEXT,"
               "email TEXT,"
               "address TEXT,"
               "reader_type TEXT DEFAULT '普通读者',"
               "max_borrow INTEGER DEFAULT 5,"
               "max_days INTEGER DEFAULT 30,"
               "status TEXT DEFAULT '正常',"
               "registration_date DATE DEFAULT CURRENT_DATE,"
               "expiry_date DATE,"
               "notes TEXT)");

    // 创建借阅记录表
    query.exec("CREATE TABLE IF NOT EXISTS borrow_records ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "book_id INTEGER NOT NULL,"
               "reader_id INTEGER NOT NULL,"
               "borrow_date DATE NOT NULL,"
               "due_date DATE NOT NULL,"
               "return_date DATE,"
               "renew_count INTEGER DEFAULT 0,"
               "status TEXT DEFAULT '借出',"
               "overdue_fee REAL DEFAULT 0,"
               "FOREIGN KEY(book_id) REFERENCES books(id),"
               "FOREIGN KEY(reader_id) REFERENCES readers(id))");

    // 创建借阅历史表
    query.exec("CREATE TABLE IF NOT EXISTS borrow_history ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "book_id INTEGER,"
               "reader_id INTEGER,"
               "action TEXT,"
               "action_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
               "details TEXT)");

    // 插入一些示例数据（如果表为空）
    query.exec("SELECT COUNT(*) FROM books");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("INSERT INTO books (isbn, title, author, publisher, category, price, total_copies, available_copies) "
                   "VALUES ('9787111636664', 'C++ Primer', 'Stanley Lippman', '机械工业出版社', '编程', 128.0, 5, 5)");
        query.exec("INSERT INTO books (isbn, title, author, publisher, category, price, total_copies, available_copies) "
                   "VALUES ('9787302518014', 'Qt5开发实战', '王维波', '清华大学出版社', '编程', 89.0, 3, 3)");
    }
}

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

void LibraryManager::createStatisticsTab()
{
    QWidget *statsTab = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(statsTab);

    // 统计信息区域
    QGroupBox *statsGroup = new QGroupBox("统计概览");
    QGridLayout *statsLayout = new QGridLayout;

    totalBooksLabel = new QLabel("总计: 0");
    totalReadersLabel = new QLabel("读者: 0");
    borrowedBooksLabel = new QLabel("已借: 0");
    overdueBooksLabel = new QLabel("逾期: 0");
    popularCategoryLabel = new QLabel("热门分类: 无");
    activeReadersLabel = new QLabel("活跃读者: 0");

    statsLayout->addWidget(totalBooksLabel, 0, 0);
    statsLayout->addWidget(totalReadersLabel, 0, 1);
    statsLayout->addWidget(borrowedBooksLabel, 0, 2);
    statsLayout->addWidget(overdueBooksLabel, 1, 0);
    statsLayout->addWidget(popularCategoryLabel, 1, 1);
    statsLayout->addWidget(activeReadersLabel, 1, 2);

    QPushButton *refreshButton = new QPushButton("刷新统计");
    connect(refreshButton, &QPushButton::clicked, this, &LibraryManager::refreshStatistics);
    statsLayout->addWidget(refreshButton, 2, 0, 1, 3);

    statsGroup->setLayout(statsLayout);
    mainLayout->addWidget(statsGroup);

    // 报告区域
    QGroupBox *reportGroup = new QGroupBox("统计报告");
    QVBoxLayout *reportLayout = new QVBoxLayout(reportGroup);

    reportTextEdit = new QTextEdit;
    reportTextEdit->setReadOnly(true);
    reportLayout->addWidget(reportTextEdit);

    QHBoxLayout *reportButtonLayout = new QHBoxLayout;
    QPushButton *generateReportButton = new QPushButton("生成报告");
    connect(generateReportButton, &QPushButton::clicked, this, &LibraryManager::generateReport);
    reportButtonLayout->addWidget(generateReportButton);

    QPushButton *printButton = new QPushButton("打印");
    connect(printButton, &QPushButton::clicked, [this]() {
        QPrinter printer;
        QPrintDialog dialog(&printer, this);
        if (dialog.exec() == QDialog::Accepted) {
            reportTextEdit->print(&printer);
        }
    });
    reportButtonLayout->addWidget(printButton);
    reportButtonLayout->addStretch();
    reportLayout->addLayout(reportButtonLayout);

    mainLayout->addWidget(reportGroup);

    tabWidget->addTab(statsTab, "统计报表");

    // 初始刷新统计
    refreshStatistics();
}

void LibraryManager::createMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");

    QAction *backupAction = new QAction("备份数据库", this);
    connect(backupAction, &QAction::triggered, this, &LibraryManager::backupDatabase);
    fileMenu->addAction(backupAction);

    QAction *restoreAction = new QAction("恢复数据库", this);
    connect(restoreAction, &QAction::triggered, this, &LibraryManager::restoreDatabase);
    fileMenu->addAction(restoreAction);

    fileMenu->addSeparator();

    QAction *exitAction = new QAction("退出", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);

    QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");

    QAction *aboutAction = new QAction("关于", this);
    connect(aboutAction, &QAction::triggered, this, &LibraryManager::about);
    helpMenu->addAction(aboutAction);
}

void LibraryManager::createToolBar()
{
    QToolBar *toolBar = addToolBar("主工具栏");

    QAction *addBookAction = new QAction(QIcon(":/icons/add.png"), "添加图书", this);
    connect(addBookAction, &QAction::triggered, this, &LibraryManager::addBook);
    toolBar->addAction(addBookAction);

    QAction *addReaderAction = new QAction(QIcon(":/icons/user_add.png"), "添加读者", this);
    connect(addReaderAction, &QAction::triggered, this, &LibraryManager::addReader);
    toolBar->addAction(addReaderAction);

    toolBar->addSeparator();

    QAction *borrowAction = new QAction(QIcon(":/icons/borrow.png"), "借书", this);
    connect(borrowAction, &QAction::triggered, this, &LibraryManager::borrowBook);
    toolBar->addAction(borrowAction);

    QAction *returnAction = new QAction(QIcon(":/icons/return.png"), "还书", this);
    connect(returnAction, &QAction::triggered, this, &LibraryManager::returnBook);
    toolBar->addAction(returnAction);

    toolBar->addSeparator();

    QAction *statsAction = new QAction(QIcon(":/icons/undo.png"), "刷新统计", this);
    connect(statsAction, &QAction::triggered, this, &LibraryManager::refreshStatistics);
    toolBar->addAction(statsAction);

    QAction *overdueAction = new QAction(QIcon(":/icons/warning.png"), "检查逾期", this);
    connect(overdueAction, &QAction::triggered, this, &LibraryManager::checkOverdueBooks);
    toolBar->addAction(overdueAction);
}

void LibraryManager::createStatusBar()
{
    statusBar()->showMessage("就绪", 5000);
}

void LibraryManager::createModels()
{
    // 统计数据模型
    statisticsModel = new QStandardItemModel(this);
}

// 图书管理槽函数
void LibraryManager::addBook()
{
    QDialog dialog(this);
    dialog.setWindowTitle("添加图书");
    QFormLayout layout(&dialog);

    QLineEdit *isbnEdit = new QLineEdit;
    QLineEdit *titleEdit = new QLineEdit;
    QLineEdit *authorEdit = new QLineEdit;
    QLineEdit *publisherEdit = new QLineEdit;
    QDateEdit *publishDateEdit = new QDateEdit(QDate::currentDate());
    QComboBox *categoryCombo = new QComboBox;
    categoryCombo->setEditable(true);
    categoryCombo->addItems({"编程", "文学", "科学", "历史", "艺术", "教育"});
    QDoubleSpinBox *priceSpin = new QDoubleSpinBox;
    priceSpin->setRange(0, 9999);
    priceSpin->setDecimals(2);
    QSpinBox *copiesSpin = new QSpinBox;
    copiesSpin->setRange(1, 1000);
    QLineEdit *locationEdit = new QLineEdit;
    QTextEdit *descEdit = new QTextEdit;

    layout.addRow("ISBN:", isbnEdit);
    layout.addRow("书名:", titleEdit);
    layout.addRow("作者:", authorEdit);
    layout.addRow("出版社:", publisherEdit);
    layout.addRow("出版日期:", publishDateEdit);
    layout.addRow("分类:", categoryCombo);
    layout.addRow("价格:", priceSpin);
    layout.addRow("数量:", copiesSpin);
    layout.addRow("位置:", locationEdit);
    layout.addRow("描述:", descEdit);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);

    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QSqlQuery query;
        query.prepare("INSERT INTO books (isbn, title, author, publisher, publish_date, "
                     "category, price, total_copies, available_copies, location, description) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(isbnEdit->text());
        query.addBindValue(titleEdit->text());
        query.addBindValue(authorEdit->text());
        query.addBindValue(publisherEdit->text());
        query.addBindValue(publishDateEdit->date());
        query.addBindValue(categoryCombo->currentText());
        query.addBindValue(priceSpin->value());
        query.addBindValue(copiesSpin->value());
        query.addBindValue(copiesSpin->value());
        query.addBindValue(locationEdit->text());
        query.addBindValue(descEdit->toPlainText());

        if (query.exec()) {
            QMessageBox::information(this, "成功", "图书添加成功！");
            bookModel->select();
            refreshStatistics();
        } else {
            QMessageBox::warning(this, "错误", "添加图书失败：" + query.lastError().text());
        }
    }
}

