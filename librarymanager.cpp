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

void LibraryManager::editBook()
{
    QModelIndexList selection = bookTableView->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择要编辑的图书！");
        return;
    }

    int row = selection.first().row();
    int bookId = bookModel->data(bookModel->index(row, 0)).toInt();

    QSqlQuery query;
    query.prepare("SELECT * FROM books WHERE id = ?");
    query.addBindValue(bookId);
    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, "错误", "未找到选择的图书！");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("编辑图书");
    QFormLayout layout(&dialog);

    QLineEdit *isbnEdit = new QLineEdit(query.value("isbn").toString());
    QLineEdit *titleEdit = new QLineEdit(query.value("title").toString());
    QLineEdit *authorEdit = new QLineEdit(query.value("author").toString());
    QLineEdit *publisherEdit = new QLineEdit(query.value("publisher").toString());
    QDateEdit *publishDateEdit = new QDateEdit(query.value("publish_date").toDate());
    QComboBox *categoryCombo = new QComboBox;
    categoryCombo->setEditable(true);
    categoryCombo->addItems({"编程", "文学", "科学", "历史", "艺术", "教育"});
    categoryCombo->setCurrentText(query.value("category").toString());
    QDoubleSpinBox *priceSpin = new QDoubleSpinBox;
    priceSpin->setRange(0, 9999);
    priceSpin->setDecimals(2);
    priceSpin->setValue(query.value("price").toDouble());
    QSpinBox *copiesSpin = new QSpinBox;
    copiesSpin->setRange(1, 1000);
    copiesSpin->setValue(query.value("total_copies").toInt());
    QLineEdit *locationEdit = new QLineEdit(query.value("location").toString());
    QTextEdit *descEdit = new QTextEdit(query.value("description").toString());
    QComboBox *statusCombo = new QComboBox;
    statusCombo->addItems({"在库", "借出", "维护中"});
    statusCombo->setCurrentText(query.value("status").toString());

    layout.addRow("ISBN:", isbnEdit);
    layout.addRow("书名:", titleEdit);
    layout.addRow("作者:", authorEdit);
    layout.addRow("出版社:", publisherEdit);
    layout.addRow("出版日期:", publishDateEdit);
    layout.addRow("分类:", categoryCombo);
    layout.addRow("价格:", priceSpin);
    layout.addRow("数量:", copiesSpin);
    layout.addRow("位置:", locationEdit);
    layout.addRow("状态:", statusCombo);
    layout.addRow("描述:", descEdit);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);

    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE books SET isbn = ?, title = ?, author = ?, publisher = ?, "
                          "publish_date = ?, category = ?, price = ?, total_copies = ?, "
                          "available_copies = ?, location = ?, status = ?, description = ? "
                          "WHERE id = ?");
        updateQuery.addBindValue(isbnEdit->text());
        updateQuery.addBindValue(titleEdit->text());
        updateQuery.addBindValue(authorEdit->text());
        updateQuery.addBindValue(publisherEdit->text());
        updateQuery.addBindValue(publishDateEdit->date());
        updateQuery.addBindValue(categoryCombo->currentText());
        updateQuery.addBindValue(priceSpin->value());
        updateQuery.addBindValue(copiesSpin->value());
        updateQuery.addBindValue(copiesSpin->value()); // 假设编辑时可用数量等于总数
        updateQuery.addBindValue(locationEdit->text());
        updateQuery.addBindValue(statusCombo->currentText());
        updateQuery.addBindValue(descEdit->toPlainText());
        updateQuery.addBindValue(bookId);

        if (updateQuery.exec()) {
            QMessageBox::information(this, "成功", "图书信息更新成功！");
            bookModel->select();
        } else {
            QMessageBox::warning(this, "错误", "更新失败：" + updateQuery.lastError().text());
        }
    }
}

void LibraryManager::deleteBook()
{
    QModelIndexList selection = bookTableView->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择要删除的图书！");
        return;
    }

    int row = selection.first().row();
    QString bookTitle = bookModel->data(bookModel->index(row, 2)).toString();

    int result = QMessageBox::question(this, "确认删除",
        QString("确定要删除图书《%1》吗？").arg(bookTitle),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        int bookId = bookModel->data(bookModel->index(row, 0)).toInt();

        // 检查图书是否被借出
        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM borrow_records WHERE book_id = ? AND status = '借出'");
        checkQuery.addBindValue(bookId);
        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            QMessageBox::warning(this, "错误", "该图书已被借出，无法删除！");
            return;
        }

        QSqlQuery deleteQuery;
        deleteQuery.prepare("DELETE FROM books WHERE id = ?");
        deleteQuery.addBindValue(bookId);

        if (deleteQuery.exec()) {
            QMessageBox::information(this, "成功", "图书删除成功！");
            bookModel->select();
            refreshStatistics();
        } else {
            QMessageBox::warning(this, "错误", "删除失败：" + deleteQuery.lastError().text());
        }
    }
}

void LibraryManager::searchBooks()
{
    QStringList filters;

    if (!bookIdFilter->text().isEmpty()) {
        filters.append(QString("id = %1").arg(bookIdFilter->text()));
    }
    if (!bookTitleFilter->text().isEmpty()) {
        filters.append(QString("title LIKE '%%1%'").arg(bookTitleFilter->text()));
    }
    if (!bookAuthorFilter->text().isEmpty()) {
        filters.append(QString("author LIKE '%%1%'").arg(bookAuthorFilter->text()));
    }
    if (!bookIsbnFilter->text().isEmpty()) {
        filters.append(QString("isbn LIKE '%%1%'").arg(bookIsbnFilter->text()));
    }
    if (bookCategoryFilter->currentText() != "所有分类") {
        filters.append(QString("category = '%1'").arg(bookCategoryFilter->currentText()));
    }
    if (bookStatusFilter->currentText() != "所有状态") {
        filters.append(QString("status = '%1'").arg(bookStatusFilter->currentText()));
    }

    QString filter = filters.join(" AND ");
    bookModel->setFilter(filter);
    bookModel->select();

    statusBar()->showMessage(QString("找到 %1 本图书").arg(bookModel->rowCount()), 3000);
}

void LibraryManager::clearBookSearch()
{
    bookIdFilter->clear();
    bookTitleFilter->clear();
    bookAuthorFilter->clear();
    bookIsbnFilter->clear();
    bookCategoryFilter->setCurrentIndex(0);
    bookStatusFilter->setCurrentIndex(0);

    bookModel->setFilter("");
    bookModel->select();
}

// 读者管理槽函数
void LibraryManager::addReader()
{
    QDialog dialog(this);
    dialog.setWindowTitle("添加读者");
    dialog.setFixedWidth(400);
    QFormLayout layout(&dialog);

    QLineEdit *cardEdit = new QLineEdit;
    QLineEdit *nameEdit = new QLineEdit;
    QComboBox *genderCombo = new QComboBox;
    genderCombo->addItems({"男", "女"});
    QDateEdit *birthDateEdit = new QDateEdit(QDate::currentDate().addYears(-20));
    birthDateEdit->setCalendarPopup(true);
    QLineEdit *phoneEdit = new QLineEdit;
    QLineEdit *emailEdit = new QLineEdit;
    QLineEdit *addressEdit = new QLineEdit;
    QComboBox *typeCombo = new QComboBox;
    typeCombo->addItems({"普通读者", "学生", "教师", "VIP"});
    QSpinBox *maxBorrowSpin = new QSpinBox;
    maxBorrowSpin->setRange(1, 20);
    maxBorrowSpin->setValue(5);
    QSpinBox *maxDaysSpin = new QSpinBox;
    maxDaysSpin->setRange(7, 180);
    maxDaysSpin->setValue(30);
    QDateEdit *expiryDateEdit = new QDateEdit(QDate::currentDate().addYears(1));
    expiryDateEdit->setCalendarPopup(true);
    QTextEdit *notesEdit = new QTextEdit;

    layout.addRow("借书证号:", cardEdit);
    layout.addRow("姓名:", nameEdit);
    layout.addRow("性别:", genderCombo);
    layout.addRow("出生日期:", birthDateEdit);
    layout.addRow("电话:", phoneEdit);
    layout.addRow("邮箱:", emailEdit);
    layout.addRow("地址:", addressEdit);
    layout.addRow("读者类型:", typeCombo);
    layout.addRow("最大借书数:", maxBorrowSpin);
    layout.addRow("最长借期:", maxDaysSpin);
    layout.addRow("有效期至:", expiryDateEdit);
    layout.addRow("备注:", notesEdit);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);

    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QSqlQuery query;
        query.prepare("INSERT INTO readers (card_number, name, gender, birth_date, phone, "
                     "email, address, reader_type, max_borrow, max_days, expiry_date, notes) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(cardEdit->text());
        query.addBindValue(nameEdit->text());
        query.addBindValue(genderCombo->currentText());
        query.addBindValue(birthDateEdit->date());
        query.addBindValue(phoneEdit->text());
        query.addBindValue(emailEdit->text());
        query.addBindValue(addressEdit->text());
        query.addBindValue(typeCombo->currentText());
        query.addBindValue(maxBorrowSpin->value());
        query.addBindValue(maxDaysSpin->value());
        query.addBindValue(expiryDateEdit->date());
        query.addBindValue(notesEdit->toPlainText());

        if (query.exec()) {
            QMessageBox::information(this, "成功", "读者添加成功！");
            readerModel->select();
            refreshStatistics();
        } else {
            QMessageBox::warning(this, "错误", "添加读者失败：" + query.lastError().text());
        }
    }
}

void LibraryManager::editReader()
{
    QModelIndexList selection = readerTableView->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择要编辑的读者！");
        return;
    }

    int row = selection.first().row();
    int readerId = readerModel->data(readerModel->index(row, 0)).toInt();

    QSqlQuery query;
    query.prepare("SELECT * FROM readers WHERE id = ?");
    query.addBindValue(readerId);
    if (!query.exec() || !query.next()) {
        QMessageBox::warning(this, "错误", "未找到选择的读者！");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("编辑读者");
    dialog.setFixedWidth(400);
    QFormLayout layout(&dialog);

    QLineEdit *cardEdit = new QLineEdit(query.value("card_number").toString());
    QLineEdit *nameEdit = new QLineEdit(query.value("name").toString());
    QComboBox *genderCombo = new QComboBox;
    genderCombo->addItems({"男", "女"});
    genderCombo->setCurrentText(query.value("gender").toString());
    QDateEdit *birthDateEdit = new QDateEdit(query.value("birth_date").toDate());
    birthDateEdit->setCalendarPopup(true);
    QLineEdit *phoneEdit = new QLineEdit(query.value("phone").toString());
    QLineEdit *emailEdit = new QLineEdit(query.value("email").toString());
    QLineEdit *addressEdit = new QLineEdit(query.value("address").toString());
    QComboBox *typeCombo = new QComboBox;
    typeCombo->addItems({"普通读者", "学生", "教师", "VIP"});
    typeCombo->setCurrentText(query.value("reader_type").toString());
    QSpinBox *maxBorrowSpin = new QSpinBox;
    maxBorrowSpin->setRange(1, 20);
    maxBorrowSpin->setValue(query.value("max_borrow").toInt());
    QSpinBox *maxDaysSpin = new QSpinBox;
    maxDaysSpin->setRange(7, 180);
    maxDaysSpin->setValue(query.value("max_days").toInt());
    QComboBox *statusCombo = new QComboBox;
    statusCombo->addItems({"正常", "挂失", "停用"});
    statusCombo->setCurrentText(query.value("status").toString());
    QDateEdit *expiryDateEdit = new QDateEdit(query.value("expiry_date").toDate());
    expiryDateEdit->setCalendarPopup(true);
    QTextEdit *notesEdit = new QTextEdit(query.value("notes").toString());

    layout.addRow("借书证号:", cardEdit);
    layout.addRow("姓名:", nameEdit);
    layout.addRow("性别:", genderCombo);
    layout.addRow("出生日期:", birthDateEdit);
    layout.addRow("电话:", phoneEdit);
    layout.addRow("邮箱:", emailEdit);
    layout.addRow("地址:", addressEdit);
    layout.addRow("读者类型:", typeCombo);
    layout.addRow("最大借书数:", maxBorrowSpin);
    layout.addRow("最长借期:", maxDaysSpin);
    layout.addRow("状态:", statusCombo);
    layout.addRow("有效期至:", expiryDateEdit);
    layout.addRow("备注:", notesEdit);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);

    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE readers SET card_number = ?, name = ?, gender = ?, "
                          "birth_date = ?, phone = ?, email = ?, address = ?, reader_type = ?, "
                          "max_borrow = ?, max_days = ?, status = ?, expiry_date = ?, notes = ? "
                          "WHERE id = ?");
        updateQuery.addBindValue(cardEdit->text());
        updateQuery.addBindValue(nameEdit->text());
        updateQuery.addBindValue(genderCombo->currentText());
        updateQuery.addBindValue(birthDateEdit->date());
        updateQuery.addBindValue(phoneEdit->text());
        updateQuery.addBindValue(emailEdit->text());
        updateQuery.addBindValue(addressEdit->text());
        updateQuery.addBindValue(typeCombo->currentText());
        updateQuery.addBindValue(maxBorrowSpin->value());
        updateQuery.addBindValue(maxDaysSpin->value());
        updateQuery.addBindValue(statusCombo->currentText());
        updateQuery.addBindValue(expiryDateEdit->date());
        updateQuery.addBindValue(notesEdit->toPlainText());
        updateQuery.addBindValue(readerId);

        if (updateQuery.exec()) {
            QMessageBox::information(this, "成功", "读者信息更新成功！");
            readerModel->select();
        } else {
            QMessageBox::warning(this, "错误", "更新失败：" + updateQuery.lastError().text());
        }
    }
}

void LibraryManager::deleteReader()
{
    QModelIndexList selection = readerTableView->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择要删除的读者！");
        return;
    }

    int row = selection.first().row();
    QString readerName = readerModel->data(readerModel->index(row, 2)).toString();

    int result = QMessageBox::question(this, "确认删除",
        QString("确定要删除读者【%1】吗？").arg(readerName),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        int readerId = readerModel->data(readerModel->index(row, 0)).toInt();

        // 检查读者是否有未归还的图书
        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM borrow_records WHERE reader_id = ? AND status = '借出'");
        checkQuery.addBindValue(readerId);
        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            QMessageBox::warning(this, "错误", "该读者有未归还的图书，无法删除！");
            return;
        }

        QSqlQuery deleteQuery;
        deleteQuery.prepare("DELETE FROM readers WHERE id = ?");
        deleteQuery.addBindValue(readerId);

        if (deleteQuery.exec()) {
            QMessageBox::information(this, "成功", "读者删除成功！");
            readerModel->select();
            refreshStatistics();
        } else {
            QMessageBox::warning(this, "错误", "删除失败：" + deleteQuery.lastError().text());
        }
    }
}

void LibraryManager::searchReaders()
{
    QStringList filters;

    if (!readerIdFilter->text().isEmpty()) {
        filters.append(QString("id = %1").arg(readerIdFilter->text()));
    }
    if (!readerNameFilter->text().isEmpty()) {
        filters.append(QString("name LIKE '%%1%'").arg(readerNameFilter->text()));
    }
    if (!readerPhoneFilter->text().isEmpty()) {
        filters.append(QString("phone LIKE '%%1%'").arg(readerPhoneFilter->text()));
    }
    if (readerTypeFilter->currentText() != "所有类型") {
        filters.append(QString("reader_type = '%1'").arg(readerTypeFilter->currentText()));
    }

    QString filter = filters.join(" AND ");
    readerModel->setFilter(filter);
    readerModel->select();

    statusBar()->showMessage(QString("找到 %1 位读者").arg(readerModel->rowCount()), 3000);
}

void LibraryManager::clearReaderSearch()
{
    readerIdFilter->clear();
    readerNameFilter->clear();
    readerPhoneFilter->clear();
    readerTypeFilter->setCurrentIndex(0);

    readerModel->setFilter("");
    readerModel->select();
}

void LibraryManager::clearReaderSearch()
{
    readerIdFilter->clear();
    readerNameFilter->clear();
    readerPhoneFilter->clear();
    readerTypeFilter->setCurrentIndex(0);

    readerModel->setFilter("");
    readerModel->select();
}

// 借还书管理槽函数
void LibraryManager::borrowBook()
{
    QString bookId = borrowBookId->text().trimmed();
    QString readerId = borrowReaderId->text().trimmed();

    if (bookId.isEmpty() || readerId.isEmpty()) {
        QMessageBox::warning(this, "错误", "请填写图书ID和读者ID！");
        return;
    }

    QSqlDatabase::database().transaction();

    try {
        // 检查图书是否存在且可借
        QSqlQuery bookQuery;
        bookQuery.prepare("SELECT id, title, available_copies FROM books WHERE id = ?");
        bookQuery.addBindValue(bookId.toInt());
        if (!bookQuery.exec() || !bookQuery.next()) {
            throw QString("图书ID不存在！");
        }

        int availableCopies = bookQuery.value("available_copies").toInt();
        if (availableCopies <= 0) {
            throw QString("该图书已全部借出！");
        }

        QString bookTitle = bookQuery.value("title").toString();

        // 检查读者是否存在且可借
        QSqlQuery readerQuery;
        readerQuery.prepare("SELECT id, name, max_borrow, status FROM readers WHERE id = ?");
        readerQuery.addBindValue(readerId.toInt());
        if (!readerQuery.exec() || !readerQuery.next()) {
            throw QString("读者ID不存在！");
        }

        if (readerQuery.value("status").toString() != "正常") {
            throw QString("该读者状态异常，无法借书！");
        }

        QString readerName = readerQuery.value("name").toString();
        int maxBorrow = readerQuery.value("max_borrow").toInt();

        // 检查读者当前借书数量
        QSqlQuery countQuery;
        countQuery.prepare("SELECT COUNT(*) FROM borrow_records WHERE reader_id = ? AND status = '借出'");
        countQuery.addBindValue(readerId.toInt());
        if (countQuery.exec() && countQuery.next() && countQuery.value(0).toInt() >= maxBorrow) {
            throw QString("该读者已达到最大借书数量限制！");
        }

        // 检查是否已借过同一本书
        QSqlQuery duplicateQuery;
        duplicateQuery.prepare("SELECT COUNT(*) FROM borrow_records WHERE book_id = ? AND reader_id = ? AND status = '借出'");
        duplicateQuery.addBindValue(bookId.toInt());
        duplicateQuery.addBindValue(readerId.toInt());
        if (duplicateQuery.exec() && duplicateQuery.next() && duplicateQuery.value(0).toInt() > 0) {
            throw QString("该读者已借阅此书，请勿重复借阅！");
        }

        // 获取最长借期
        QSqlQuery maxDaysQuery;
        maxDaysQuery.prepare("SELECT max_days FROM readers WHERE id = ?");
        maxDaysQuery.addBindValue(readerId.toInt());
        int maxDays = 30; // 默认30天
        if (maxDaysQuery.exec() && maxDaysQuery.next()) {
            maxDays = maxDaysQuery.value(0).toInt();
        }

        int borrowDays = this->borrowDays->value();
        if (borrowDays > maxDays) {
            borrowDays = maxDays;
        }

        QDate borrowDate = QDate::currentDate();
        QDate dueDate = borrowDate.addDays(borrowDays);

        // 插入借阅记录
        QSqlQuery borrowQuery;
        borrowQuery.prepare("INSERT INTO borrow_records (book_id, reader_id, borrow_date, due_date) "
                          "VALUES (?, ?, ?, ?)");
        borrowQuery.addBindValue(bookId.toInt());
        borrowQuery.addBindValue(readerId.toInt());
        borrowQuery.addBindValue(borrowDate);
        borrowQuery.addBindValue(dueDate);

        if (!borrowQuery.exec()) {
            throw QString("借阅记录创建失败：" + borrowQuery.lastError().text());
        }

        // 更新图书可用数量
        QSqlQuery updateBookQuery;
        updateBookQuery.prepare("UPDATE books SET available_copies = available_copies - 1 WHERE id = ?");
        updateBookQuery.addBindValue(bookId.toInt());

        if (!updateBookQuery.exec()) {
            throw QString("更新图书信息失败：" + updateBookQuery.lastError().text());
        }

        // 记录历史
        QSqlQuery historyQuery;
        historyQuery.prepare("INSERT INTO borrow_history (book_id, reader_id, action, details) "
                           "VALUES (?, ?, ?, ?)");
        historyQuery.addBindValue(bookId.toInt());
        historyQuery.addBindValue(readerId.toInt());
        historyQuery.addBindValue("借出");
        historyQuery.addBindValue(QString("借阅《%1》，应还日期：%2").arg(bookTitle).arg(dueDate.toString("yyyy-MM-dd")));

        historyQuery.exec();

        QSqlDatabase::database().commit();

        QMessageBox::information(this, "成功",
            QString("借书成功！\n图书：%1\n读者：%2\n应还日期：%3")
                .arg(bookTitle)
                .arg(readerName)
                .arg(dueDate.toString("yyyy-MM-dd")));

        // 清空输入框
        borrowBookId->clear();
        borrowReaderId->clear();

        // 刷新显示
        bookModel->select();
        borrowModel->select();
        refreshStatistics();

    } catch (const QString &error) {
        QSqlDatabase::database().rollback();
        QMessageBox::warning(this, "借书失败", error);
    }
}

void LibraryManager::returnBook()
{
    QString recordId = returnRecordId->text().trimmed();

    if (recordId.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入借阅记录ID！");
        return;
    }

    QSqlDatabase::database().transaction();

    try {
        // 检查借阅记录
        QSqlQuery borrowQuery;
        borrowQuery.prepare("SELECT br.*, b.title, r.name FROM borrow_records br "
                          "JOIN books b ON br.book_id = b.id "
                          "JOIN readers r ON br.reader_id = r.id "
                          "WHERE br.id = ? AND br.status = '借出'");
        borrowQuery.addBindValue(recordId.toInt());

        if (!borrowQuery.exec() || !borrowQuery.next()) {
            throw QString("无效的借阅记录ID或图书已归还！");
        }

        int bookId = borrowQuery.value("book_id").toInt();
        QDate dueDate = borrowQuery.value("due_date").toDate();
        QDate returnDate = QDate::currentDate();

        // 计算逾期天数和费用
        int overdueDays = 0;
        double overdueFee = 0.0;

        if (returnDate > dueDate) {
            overdueDays = dueDate.daysTo(returnDate);
            overdueFee = overdueDays * 0.5; // 每天0.5元逾期费
        }

        // 更新借阅记录
        QSqlQuery updateBorrowQuery;
        updateBorrowQuery.prepare("UPDATE borrow_records SET return_date = ?, status = '已还', "
                              "overdue_fee = ? WHERE id = ?");
        updateBorrowQuery.addBindValue(returnDate);
        updateBorrowQuery.addBindValue(overdueFee);
        updateBorrowQuery.addBindValue(recordId.toInt());

        if (!updateBorrowQuery.exec()) {
            throw QString("更新借阅记录失败！");
        }

        // 更新图书可用数量
        QSqlQuery updateBookQuery;
        updateBookQuery.prepare("UPDATE books SET available_copies = available_copies + 1 WHERE id = ?");
        updateBookQuery.addBindValue(bookId);

        if (!updateBookQuery.exec()) {
            throw QString("更新图书信息失败！");
        }

        // 记录历史
        QString bookTitle = borrowQuery.value("title").toString();
        QString readerName = borrowQuery.value("name").toString();

        QSqlQuery historyQuery;
        historyQuery.prepare("INSERT INTO borrow_history (book_id, reader_id, action, details) "
                           "VALUES (?, ?, ?, ?)");
        historyQuery.addBindValue(bookId);
        historyQuery.addBindValue(borrowQuery.value("reader_id").toInt());
        historyQuery.addBindValue("归还");
        QString details = QString("归还《%1》").arg(bookTitle);
        if (overdueDays > 0) {
            details += QString("，逾期%1天，费用：%2元").arg(overdueDays).arg(overdueFee, 0, 'f', 2);
        }
        historyQuery.addBindValue(details);

        historyQuery.exec();

        QSqlDatabase::database().commit();

        QString message = QString("还书成功！\n图书：%1\n读者：%2")
                          .arg(bookTitle)
                          .arg(readerName);

        if (overdueDays > 0) {
            message += QString("\n逾期%1天，需支付费用：%2元")
                      .arg(overdueDays)
                      .arg(overdueFee, 0, 'f', 2);
        }

        QMessageBox::information(this, "成功", message);

        // 清空输入框
        returnRecordId->clear();

        // 刷新显示
        bookModel->select();
        borrowModel->select();
        refreshStatistics();

    } catch (const QString &error) {
        QSqlDatabase::database().rollback();
        QMessageBox::warning(this, "还书失败", error);
    }
}

void LibraryManager::renewBook()
{
    QString recordId = returnRecordId->text().trimmed();

    if (recordId.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入借阅记录ID！");
        return;
    }

    QSqlDatabase::database().transaction();

    try {
        // 检查借阅记录
        QSqlQuery borrowQuery;
        borrowQuery.prepare("SELECT br.*, b.title, r.name, r.max_days FROM borrow_records br "
                          "JOIN books b ON br.book_id = b.id "
                          "JOIN readers r ON br.reader_id = r.id "
                          "WHERE br.id = ? AND br.status = '借出'");
        borrowQuery.addBindValue(recordId.toInt());

        if (!borrowQuery.exec() || !borrowQuery.next()) {
            throw QString("无效的借阅记录ID或图书已归还！");
        }

        int renewCount = borrowQuery.value("renew_count").toInt();
        if (renewCount >= 2) { // 最多续借2次
            throw QString("该书已续借2次，无法再次续借！");
        }

        QDate currentDueDate = borrowQuery.value("due_date").toDate();
        int maxDays = borrowQuery.value("max_days").toInt();
        QDate newDueDate = QDate::currentDate().addDays(maxDays);

        if (newDueDate <= currentDueDate) {
            throw QString("续借后日期必须晚于当前应还日期！");
        }

        // 更新借阅记录
        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE borrow_records SET due_date = ?, renew_count = renew_count + 1 WHERE id = ?");
        updateQuery.addBindValue(newDueDate);
        updateQuery.addBindValue(recordId.toInt());

        if (!updateQuery.exec()) {
            throw QString("续借失败！");
        }

        // 记录历史
        QString bookTitle = borrowQuery.value("title").toString();
        QString readerName = borrowQuery.value("name").toString();

        QSqlQuery historyQuery;
        historyQuery.prepare("INSERT INTO borrow_history (book_id, reader_id, action, details) "
                           "VALUES (?, ?, ?, ?)");
        historyQuery.addBindValue(borrowQuery.value("book_id").toInt());
        historyQuery.addBindValue(borrowQuery.value("reader_id").toInt());
        historyQuery.addBindValue("续借");
        historyQuery.addBindValue(QString("续借《%1》至%2").arg(bookTitle).arg(newDueDate.toString("yyyy-MM-dd")));

        historyQuery.exec();

        QSqlDatabase::database().commit();

        QMessageBox::information(this, "成功",
            QString("续借成功！\n图书：%1\n读者：%2\n新应还日期：%3")
                .arg(bookTitle)
                .arg(readerName)
                .arg(newDueDate.toString("yyyy-MM-dd")));

        // 清空输入框
        returnRecordId->clear();

        // 刷新显示
        borrowModel->select();

    } catch (const QString &error) {
        QSqlDatabase::database().rollback();
        QMessageBox::warning(this, "续借失败", error);
    }
}

// 统计功能
void LibraryManager::refreshStatistics()
{
    QSqlQuery query;

    // 总图书数量
    query.exec("SELECT COUNT(*) FROM books");
    if (query.next()) {
        totalBooksLabel->setText(QString("总计: %1 本").arg(query.value(0).toInt()));
    }

    // 总读者数量
    query.exec("SELECT COUNT(*) FROM readers");
    if (query.next()) {
        totalReadersLabel->setText(QString("读者: %1 人").arg(query.value(0).toInt()));
    }

    // 已借出图书数量
    query.exec("SELECT COUNT(*) FROM borrow_records WHERE status = '借出'");
    if (query.next()) {
        borrowedBooksLabel->setText(QString("已借: %1 本").arg(query.value(0).toInt()));
    }

    // 逾期图书数量
    query.exec("SELECT COUNT(*) FROM borrow_records WHERE status = '借出' AND due_date < date('now')");
    if (query.next()) {
        overdueBooksLabel->setText(QString("逾期: %1 本").arg(query.value(0).toInt()));
    }

    // 热门分类
    query.exec("SELECT category, COUNT(*) as count FROM books GROUP BY category ORDER BY count DESC LIMIT 1");
    if (query.next()) {
        popularCategoryLabel->setText(QString("热门分类: %1").arg(query.value(0).toString()));
    }

    // 活跃读者（最近30天有借书记录的）
    query.exec("SELECT COUNT(DISTINCT reader_id) FROM borrow_records "
               "WHERE borrow_date >= date('now', '-30 days')");
    if (query.next()) {
        activeReadersLabel->setText(QString("活跃读者: %1 人").arg(query.value(0).toInt()));
    }
}













