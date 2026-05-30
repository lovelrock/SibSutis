from flask import Flask, request, render_template, jsonify
import psycopg2
from psycopg2.extras import RealDictCursor
import hashlib

app = Flask(__name__)

# Конфигурация БД
DB_CONFIG = {
    'dbname': 'bank_db',
    'user': 'bank_user',
    'password': 'bank123',
    'host': 'localhost',
    'port': 5432
}

def get_db_connection():
    """Создание подключения к PostgreSQL"""
    conn = psycopg2.connect(**DB_CONFIG)
    return conn

@app.route('/', methods=['GET', 'POST'])
def index():
    user_data = None
    error = None
    
    if request.method == 'POST':
        email = request.form.get('email', '').strip()
        birth_date = request.form.get('birth_date', '').strip()
        
        # УЯЗВИМОСТЬ: Прямая подстановка параметра email в SQL-запрос
        # Это позволяет выполнить UNION-based SQL injection
        query = f"""
            SELECT 
                login, 
                password_hash, 
                email, 
                full_name, 
                TO_CHAR(birth_date, 'YYYY-MM-DD') as birth_date,
                gender, 
                balance::text
            FROM users 
            WHERE email = '{email}' AND birth_date = '{birth_date}'
        """
        
        print(f"\n🔍 Выполняется запрос:")
        print(f"   {query}")
        
        conn = None
        try:
            conn = get_db_connection()
            cur = conn.cursor(cursor_factory=RealDictCursor)
            cur.execute(query)
            row = cur.fetchone()
            
            if row:
                user_data = dict(row)
                print(f"   Найден пользователь: {user_data.get('login')}")
            else:
                error = " Пользователь с указанными email и датой рождения не найден"
                print(f"    Пользователь не найден")
                
            cur.close()
            
        except Exception as e:
            error = f"Ошибка выполнения запроса: {str(e)}"
            print(f"   Ошибка: {e}")
        finally:
            if conn:
                conn.close()
    
    return render_template('index.html', user=user_data, error=error)

@app.route('/health', methods=['GET'])
def health():
    """Проверка работы приложения"""
    return jsonify({"status": "ok", "database": "PostgreSQL"})

if __name__ == '__main__':
    print(" Запуск веб-приложения на http://localhost:5000")
    app.run(debug=True, host='0.0.0.0', port=5000)
