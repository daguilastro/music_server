"""
Cliente WebSocket para Streaming de Música
"""

import asyncio
import websockets
import json

# Configuración de conexión (hardcoded)
SERVER_IP = "192.168.0.11"
SERVER_PORT = 42069
SERVER_URL = f"ws://{SERVER_IP}:{SERVER_PORT}"


async def enviar_numeros(websocket):
    """Envía números del 1 al 10 millones lo más rápido posible."""
    for i in range(1, 3000000):
        await websocket.send(str(i))
        
        # Opcional: mostrar progreso cada millón
        if i % 1_000_000 == 0:
            print(f"Enviados: {i:,} números")


async def recibir_mensajes(websocket):
    """Recibe mensajes del servidor continuamente."""
    async for mensaje in websocket:
        print(f"[SERVIDOR] {mensaje}")


async def conectar_servidor():
    """Conecta al servidor WebSocket."""
    print("Iniciando cliente...")
    print(f"Conectando a: {SERVER_URL}")
    
    try:
        async with websockets.connect(SERVER_URL) as websocket:
            print("Conectado!\n")
            
            # Crear dos tareas que corren en paralelo
            tarea_enviar = asyncio.create_task(enviar_numeros(websocket))
            tarea_recibir = asyncio.create_task(recibir_mensajes(websocket))
            
            # Esperar a que ambas terminen (o una falle)
            await asyncio.gather(tarea_enviar, tarea_recibir)
    
    except KeyboardInterrupt:
        print("\nDesconectando...")
    except Exception as e:
        print(f"Error: {e}")


if __name__ == "__main__":
    asyncio.run(conectar_servidor())