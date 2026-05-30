import psycopg2
from psycopg2 import sql
import hashlib

# Параметры подключения
DB_CONFIG = {
    'dbname': 'bank_db',
    'user': 'bank_user',
    'password': 'bank123',
    'host': 'localhost',
    'port': 5432
}

def hash_password(password):
    """Хэширование пароля (SHA-256 для демонстрации)"""
    return hashlib.sha256(password.encode()).hexdigest()

def init_database():
    conn = None
    try:
        # Подключение
        conn = psycopg2.connect(**DB_CONFIG)
        cur = conn.cursor()
        
        # Создание таблицы users
        cur.execute("""
            DROP TABLE IF EXISTS users CASCADE;
        """)
        
        cur.execute("""
            CREATE TABLE users (
                id SERIAL PRIMARY KEY,
                login VARCHAR(50) NOT NULL UNIQUE,
                password_hash VARCHAR(64) NOT NULL,
                email VARCHAR(100) NOT NULL,
                full_name VARCHAR(100) NOT NULL,
                birth_date DATE NOT NULL,
                gender VARCHAR(10) NOT NULL CHECK (gender IN ('male', 'female')),
                balance DECIMAL(12, 2) NOT NULL DEFAULT 0.00
            );
        """)
        
        # Тестовые данные
        test_users = [
            ('alice', 'password123', 'alice@bank.com', 'Алиса Иванова', '1990-05-15', 'female', 15000.00),
            ('bob', 'password123', 'bob@bank.com', 'Боб Смирнов', '1985-08-22', 'male', 8750.50),
            ('carol', 'password123', 'carol@bank.com', 'Каролина Петрова', '1992-11-30', 'female', 23400.75),
            ('david', 'password123', 'david@bank.com', 'Давид Козлов', '1988-03-10', 'male', 5200.25),
            ('elena', 'password123', 'elena@bank.com', 'Елена Морозова', '1995-07-19', 'female', 18900.00)
        ]
        
        for user in test_users:
            login, pwd, email, full_name, birth_date, gender, balance = user
            password_hash = hash_password(pwd)
            
            cur.execute("""
                INSERT INTO users (login, password_hash, email, full_name, birth_date, gender, balance)
                VALUES (%s, %s, %s, %s, %s, %s, %s)
            """, (login, password_hash, email, full_name, birth_date, gender, balance))
        
        conn.commit()
        
        # Проверка созданных данных
        cur.execute("SELECT COUNT(*) FROM users")
        count = cur.fetchone()[0]
        print(f"✅ База данных инициализирована. Создано {count} пользователей.")
        
        # Вывод списка пользователей для проверки
        cur.execute("SELECT login, email, full_name, birth_date FROM users")
        print("\n📋 Список пользователей:")
        for row in cur.fetchall():
            print(f"  - {row[0]} | {row[1]} | {row[2]} | {row[3]}")
        
        cur.close()
        
    except Exception as e:
        print(f"❌ Ошибка: {e}")
        if conn:
            conn.rollback()
    finally:
        if conn:
            conn.close()

if __name__ == "__main__":
    print("🚀 Инициализация базы данных PostgreSQL...")
    init_database()
