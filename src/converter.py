from PIL import Image
import struct
import os
import sys
import tkinter as tk
from tkinter import filedialog
from pydub import AudioSegment

# НАСТРОЙКИ (Константы размеров и вывода)
OUTPUT_DATA = "animation.dat"
OUTPUT_AUDIO_NAME = "sound.wav" # Имя итогового аудиофайла
OUTPUT_CONFIG = "config.ini"

# PSP Debug screen size
WIDTH = 60
HEIGHT = 34

# Палитра
ASCII_CHARS = "   :;i1tfrxvunzjJYLQ0OZmwqpkhao*MW&%B8#@"
DARK_THRESHOLD = 40

def select_files():
    """Открывает диалоговые окна для выбора файлов"""
    root = tk.Tk()
    root.withdraw() # Скрываем основное окно tkinter
    root.attributes("-topmost", True) 

    print("--- Шаг 1: Выберите GIF файл ---")
    gif_path = filedialog.askopenfilename(
        title="Выберите GIF-анимацию",
        filetypes=[("GIF files", "*.gif"), ("All files", "*.*")]
    )
    
    if not gif_path:
        print("GIF файл не выбран. Отмена.")
        sys.exit()

    print("--- Шаг 2: Выберите Аудио файл ---")
    audio_path = filedialog.askopenfilename(
        title="Выберите аудио-файл",
        filetypes=[("Audio files", "*.wav;*.mp3;*.ogg;*.flac"), ("All files", "*.*")]
    )

    if not audio_path:
        print("Аудио файл не выбран. Отмена.")
        sys.exit()
        
    return gif_path, audio_path

def process_audio(input_path, output_filename):
    """Конвертирует аудио в 44100Hz, 16-bit, Stereo"""
    print(f"Обработка аудио: {os.path.basename(input_path)}...")
    
    try:
        audio = AudioSegment.from_file(input_path)
        
        # Конвертация
        audio = audio.set_frame_rate(44100)
        audio = audio.set_channels(2)       # Стерео
        audio = audio.set_sample_width(2)   # 2 байта = 16 бит
        
        # Экспорт
        audio.export(output_filename, format="wav")
        print(f"Аудио успешно конвертировано и сохранено как: {output_filename}")
        return output_filename
    except Exception as e:
        print(f"Ошибка при обработке аудио: {e}")
        print("Убедитесь, что установлен FFmpeg, если используете форматы отличные от WAV.")
        sys.exit()

def process_image(img):
    """Ресайз и конвертация в ASCII"""
    # Ресайз с сохранением пропорций
    img_ratio = img.width / img.height
    tgt_ratio = WIDTH / HEIGHT
    
    if img_ratio > tgt_ratio:
        new_w = WIDTH
        new_h = int(WIDTH / img_ratio)
    else:
        new_h = HEIGHT
        new_w = int(HEIGHT * img_ratio)
        
    img = img.resize((new_w, new_h), Image.Resampling.BILINEAR)
    
    # Создаем черный фон и вставляем по центру
    new_img = Image.new('L', (WIDTH, HEIGHT), 0)
    paste_x = (WIDTH - new_w) // 2
    paste_y = (HEIGHT - new_h) // 2
    new_img.paste(img.convert('L'), (paste_x, paste_y))
    
    pixels = list(new_img.getdata())
    chars_len = len(ASCII_CHARS)
    
    buffer = bytearray()
    
    for y in range(HEIGHT):
        for x in range(WIDTH):
            val = pixels[y * WIDTH + x]
            if val < DARK_THRESHOLD:
                buffer.append(32) # Пробел
            else:
                idx = (val - DARK_THRESHOLD) * (chars_len - 1) // (255 - DARK_THRESHOLD)
                buffer.append(ord(ASCII_CHARS[max(0, min(idx, chars_len - 1))]))
        
        # Null-терминатор в конце строки
        buffer.append(0)
        
    return buffer

def main():
    # 1. Выбор файлов через диалог
    gif_path, raw_audio_path = select_files()
    
    # 2. Конвертация аудио
    final_audio_name = process_audio(raw_audio_path, OUTPUT_AUDIO_NAME)

    # 3. Обработка GIF
    if not os.path.exists(gif_path):
        print(f"Файл {gif_path} не найден.")
        return

    img = Image.open(gif_path)
    frames_data = bytearray()
    frame_count = 0
    
    print("Обработка кадров анимации...")
    
    try:
        while True:
            # Обработка прозрачности и конвертация
            frame = img.convert('RGBA')
            bg = Image.new('RGB', frame.size, (0,0,0))
            bg.paste(frame, mask=frame.split()[3])
            
            # Конвертация в байты (с \0 в конце строк)
            frame_bytes = process_image(bg)
            frames_data.extend(frame_bytes)
            
            frame_count += 1
            print(f"Кадр: {frame_count}", end='\r')
            
            img.seek(img.tell() + 1)
    except EOFError:
        pass

    print(f"\nГотово. Всего кадров: {frame_count}")

    # Запись бинарника
    # Header: Frames(4), Width(2), Height(2)
    with open(OUTPUT_DATA, "wb") as f:
        f.write(struct.pack("<IHH", frame_count, WIDTH, HEIGHT))
        f.write(frames_data)

    # Создание конфига
    # Используем имя сконвертированного файла
    config_text = f"""[Animation]
File = {OUTPUT_DATA}

[Audio]
File = {final_audio_name}
Volume = 100

[Display]
FrameDelay = 3
Loop = 1
"""
    with open(OUTPUT_CONFIG, "w") as f:
        f.write(config_text)
        
    print("-" * 30)
    print(f"Успешно сохранено!")
    print(f"1. Анимация: {OUTPUT_DATA} ({len(frames_data)/1024/1024:.2f} MB)")
    print(f"2. Аудио:    {final_audio_name}")
    print(f"3. Конфиг:   {OUTPUT_CONFIG}")

if __name__ == "__main__":
    main()