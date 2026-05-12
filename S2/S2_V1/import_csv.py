import csv
import tkinter as tk
from tkinter import filedialog

# Función para abrir un cuadro de diálogo y obtener la ruta del archivo CSV seleccionado
def seleccionar_archivo():
    ruta_archivo = filedialog.askopenfilename(title="Seleccionar archivo CSV", filetypes=(("Archivos CSV", "*.csv"), ("Todos los archivos", "*.*")))
    return ruta_archivo

# Definir los rangos de latitud y longitud para España
lat_min = 27.0
lat_max = 43.0
lon_min = -18.0
lon_max = 4.0

# Crear una ventana de Tkinter
root = tk.Tk()
root.withdraw()  # Ocultar la ventana principal

# Obtener la ruta del archivo CSV seleccionado
ruta_archivo_csv = seleccionar_archivo()

if ruta_archivo_csv:
    # Abrir el archivo CSV seleccionado
    with open(ruta_archivo_csv, newline='') as csvfile:
        reader = csv.reader(csvfile)
        # Iterar sobre cada fila del archivo CSV
        for row in reader:
            # Obtener las coordenadas geográficas y el tipo de la ubicación
            latitud = float(row[0])
            longitud = float(row[1])
            tipo = row[3]  # Tomar la cuarta columna como el tipo de ubicación
            # Filtrar las ubicaciones que corresponden a España
            if lat_min <= latitud <= lat_max and lon_min <= longitud <= lon_max:
                print(f"Latitud: {latitud}, Longitud: {longitud}, Tipo: {tipo}")
else:
    print("No se ha seleccionado ningún archivo CSV.")
