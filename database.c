#include "main.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

sqlite3 *db;

extern pthread_mutex_t g_db_mutex;

int Database_Init(const char* db_name)
{
    int rc = sqlite3_open(db_name, &db);
    if (rc) {
        fprintf(stderr, "Lỗi: Không thể mở database: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    // Câu lệnh SQL tạo bảng (chỉ tạo nếu chưa tồn tại)
    // const char *sql_create_table = 
    //     "CREATE TABLE IF NOT EXISTS Users("
    //     "ID INTEGER PRIMARY KEY,"
    //     "Name TEXT NOT NULL);";

    // char *err_msg = 0;
    // rc = sqlite3_exec(db, sql_create_table, 0, 0, &err_msg);
    
    // if (rc != SQLITE_OK) {
    //     fprintf(stderr, "Lỗi tạo bảng: %s\n", err_msg);
    //     sqlite3_free(err_msg);
    // } else {
    //     printf("Database '%s' và bảng Users đã sẵn sàng.\n", db_name);
    // }
    
    return rc;
}

void Database_End(void)
{
	sqlite3_close(db);
	printf("Closed SQLite3 database");
}

int get_user_name(int id, char* output_name) 
{
    sqlite3_stmt *stmt;
    // Dấu ? là tham số sẽ được truyền (bind) vào sau
    // const char *sql_query = "SELECT Name FROM " STR(DATABASE_TABLE_NAME) " WHERE ID = ?;";
	const char *sql_query = "SELECT Name FROM memberlst WHERE ID = ?;";
    int rc;
    int found = 0;

    // Gán giá trị mặc định nếu không tìm thấy người dùng
    strncpy(output_name, "Unknown", MAX_NAME_LEN);

    // B1: Biên dịch (Prepare) câu lệnh SQL
    rc = sqlite3_prepare_v2(db, sql_query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Lỗi chuẩn bị truy vấn: %s\n", sqlite3_errmsg(db));
        return 0; // Thất bại
    }

    // B2: Gắn (Bind) biến 'id' vào vị trí dấu '?' (vị trí số 1)
    sqlite3_bind_int(stmt, 1, id);

    // B3: Thực thi truy vấn và đọc từng hàng (Step)
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) // Tìm thấy bản ghi
	{ 
        const unsigned char *name = sqlite3_column_text(stmt, 0); // Đọc cột 0 (Name)

        if (name) 
		{
            strncpy(output_name, (const char*)name, MAX_NAME_LEN - 1);
            output_name[MAX_NAME_LEN - 1] = '\0'; // Đảm bảo chuỗi kết thúc đúng chuẩn C
            found = 1;
        }
    }

    // B4: Xóa statement để giải phóng bộ nhớ
    sqlite3_finalize(stmt);
    
    return found; // Trả về 1 nếu có tên, 0 nếu Unknown
}

int add_new_member(int id, const char* name) {
    sqlite3_stmt *stmt;

	pthread_mutex_lock(&g_db_mutex);
    // Dùng 2 dấu ? cho ID và Name
    const char *sql_insert = "INSERT INTO memberlst (ID, Name) VALUES (?, ?);";
    int rc;

    // B1: Biên dịch câu lệnh SQL
    rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Lỗi chuẩn bị truy vấn Insert: %s\n", sqlite3_errmsg(db));
		pthread_mutex_unlock(&g_db_mutex);
        return 0;
    }

    // B2: Gắn (Bind) giá trị vào các dấu ?
    sqlite3_bind_int(stmt, 1, id); // Dấu ? thứ nhất (ID)
    
    // Dấu ? thứ hai (Name). Tham số SQLITE_STATIC báo cho SQLite biết 
    // chuỗi name sẽ không bị thay đổi bộ nhớ trong quá trình thực thi.
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC); 

    // B3: Thực thi truy vấn
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Lỗi khi thêm user (Có thể trùng ID): %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
		pthread_mutex_unlock(&g_db_mutex);
        return 0;
    }

    // B4: Dọn dẹp
    sqlite3_finalize(stmt);
    printf("Đã thêm thành công: ID = %d, Name = %s\n", id, name);

	pthread_mutex_unlock(&g_db_mutex);

    return 1;
}

int delete_member(int id) 
{
	sqlite3_stmt *stmt;

    pthread_mutex_lock(&g_db_mutex);
    const char *sql_delete = "DELETE FROM memberlst WHERE ID = ?;";
    
    if (sqlite3_prepare_v2(db, sql_delete, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        printf("Da xoa thanh vien co ID = %d khoi database.\n", id);
    }
	else
	{
		printf("CANNOT DELETE thanh vien co ID = %d khoi database.\n", id);
		pthread_mutex_unlock(&g_db_mutex);
    	return 0;
	}
    
    pthread_mutex_unlock(&g_db_mutex);
    return 1;
}
