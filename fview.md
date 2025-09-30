# fview

`fview` es un comando para sistemas operativos basados en el kernel de Linux.
Es utilizado para monitorizar archivos en un directorio.

## ¿Como funciona?

Se requiere pasar por argumento la ruta de un directorio.

```bash
fview /home/user
```

## ¿Que informacion debe devolver?

Si el comando se ejecuta sin ningun flag, devolvera por defecto un archivo que cumpla con las siguientes condiciones:

- Mayor numero de veces modificado
- Mayor numero de veces abierto

Luego de elegir el archivo que cumpla estas 2 condiciones, se devuelve junto a esta informacion:

- Nombre del archivo
- Extension
- Ruta completa
- Los permisos de este
- Peso del archivo
- Veces que se ha modificado \- Ultima vez que se modifico
- Veces que se ha abierto \- Ultima vez que se abrio

```txt
Nombre: file
Extension: txt
Ruta: ~/Documents
Permisos: 0777
Peso: 57 KB
Veces modificado: 5 - Ultima modificacion: 2025-09-30 12:00:00 -0500
Veces abierto: 4 - Ultima vez abierto: 2025-09-30 12:00:00 -0500
```

## Flags

El comportamiento del comando puede variar segun la flag utilizada.
Este comando cuenta con algunas flags, dentro de las mas importantes tenemos:

`-m`: Muestra solo el archivo mas modificado.
`-o`: Muestra solo el archivo mas abierto.
`-l`: Muestra solo el archivo mas ligero.
`-h`: Muestra solo el archivo mas pesado.

Ejecutar el comando con todas las flags `-molh`, es el equivalente a ejecutarlo sin flags.
Por cada flag, sera una condicion mas que un archivo debe cumplir.

```bash
fview /home/user -ml
```

### Otras flags

Ademas de las flags mas importantes, tambien tenemos algunas otras flags con mucha utilidad:

`-v`: Informacion verbose
`-r`: Inspecciona de manera recursiva los subdirectorios existentes dentro del directorio inicial
`-n`: Especifica un rango de archivos para mostrar

#### Ejemplo

```bash
fview /home/user -mo -n 5
```

```txt
Nombre: file1
Extension: txt
Ruta: /home/user
Permisos: 0500
Peso: 40 KB
Veces modificado: 10 - Ultima modificacion: 2025-09-30 12:00:00 -0500
Veces abierto: 20 - Ultima vez abierto: 2025-09-30 12:00:00 -0500
```
