"""
Servidor WebSocket con UPnP - Versión Simplificada para Streaming de Música
"""

import logging
import ipaddress
import socket
import warnings
import requests
import upnpclient
import urllib3
import asyncio
import websockets
import json
from datetime import datetime
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

# Configuración
PUERTO = 42069
SERVICIOS_WAN = ['WANIPConnection', 'WANIPConn1', 'WANPPPConnection', 'WANPPPConn1']

# Configurar logging
warnings.filterwarnings("ignore")
urllib3.disable_warnings()
logging.basicConfig(level=logging.INFO, format='%(message)s')
logger = logging.getLogger(__name__)
logging.getLogger('urllib3').setLevel(logging.CRITICAL)
logging.getLogger('upnpclient').setLevel(logging.CRITICAL)

# Crear cliente HTTP con timeouts
session = requests.Session()
adapter = HTTPAdapter(max_retries=Retry(connect=1, read=1))
session.mount('http://', adapter)
session.mount('https://', adapter)

# Almacenamiento de clientes conectados
clientes_conectados = set()
estadisticas = {
    "total_conexiones": 0,
    "mensajes_recibidos": 0,
    "inicio_servidor": None
}


def es_ip_publica(ip):
    """Verifica si una IP es pública (no privada ni local)."""
    try:
        ip_obj = ipaddress.ip_address(ip)
        return not (ip_obj.is_private or ip_obj.is_loopback)
    except ValueError:
        return False


def obtener_ip_local():
    """Obtiene la IP local de tu computadora en la red."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("8.8.8.8", 80))
    ip = s.getsockname()[0]
    s.close()
    return ip


def obtener_ip_publica_real():
    """Obtiene tu IP pública real preguntándole a un servicio de internet."""
    try:
        resp = session.get('https://ifconfig.me', timeout=2)
        return resp.text.strip()
    except:
        return None


def buscar_routers():
    """Busca routers UPnP en tu red local."""
    logger.info("Buscando routers UPnP...")
    try:
        dispositivos = upnpclient.discover(timeout=1)
        logger.info(f"Dispositivos encontrados: {len(dispositivos)}")
        
        routers = [d for d in dispositivos if "InternetGatewayDevice" in d.device_type]
        logger.info(f"Routers encontrados: {len(routers)}")
        
        return routers
    except Exception as e:
        logger.error(f"Error buscando routers: {e}")
        return []


def obtener_servicio_wan(router):
    """Intenta obtener el servicio WAN del router (para controlar puertos)."""
    for nombre in SERVICIOS_WAN:
        try:
            servicio = getattr(router, nombre)
            logger.info(f"  ✓ Servicio encontrado: {nombre}")
            return servicio, nombre
        except:
            continue
    return None, None


def probar_router(router, numero):
    """Prueba si un router es válido y tiene IP pública."""
    logger.info(f"\nProbando router {numero}: {router.friendly_name}")
    
    servicio_wan, nombre_servicio = obtener_servicio_wan(router)
    if not servicio_wan:
        logger.error("  ✗ No tiene servicio WAN")
        return None
    
    try:
        result = servicio_wan.GetExternalIPAddress()
        ip_router = result['NewExternalIPAddress']
        logger.info(f"  IP del router: {ip_router}")
    except Exception as e:
        logger.error(f"  ✗ Error obteniendo IP: {e}")
        return None
    
    if not es_ip_publica(ip_router):
        logger.error("  ✗ IP no es pública")
        return None
    
    logger.info("  ✓ IP pública válida")
    
    ip_real = obtener_ip_publica_real()
    if ip_real and ip_router != ip_real:
        logger.info(f"  IP verificada externamente: {ip_real}")
        ip_router = ip_real
    
    return {
        'router': router,
        'servicio': nombre_servicio,
        'ip': ip_router,
        'nombre': router.friendly_name,
        'fabricante': router.manufacturer
    }


def abrir_puerto(info_router, ip_local):
    """Abre el puerto en el router usando UPnP."""
    logger.info(f"\nAbriendo puerto {PUERTO}...")
    
    try:
        router = info_router['router']
        servicio_wan = getattr(router, info_router['servicio'])
        
        servicio_wan.AddPortMapping(
            NewRemoteHost='',
            NewExternalPort=str(PUERTO),
            NewProtocol='TCP',
            NewInternalPort=str(PUERTO),
            NewInternalClient=ip_local,
            NewEnabled='1',
            NewPortMappingDescription='Servidor Streaming',
            NewLeaseDuration='0'
        )
        
        logger.info("  ✓ Puerto abierto exitosamente")
        return True
    except Exception as e:
        logger.error(f"  ✗ Error abriendo puerto: {e}")
        return False


async def broadcast(mensaje, remitente=None):
    """
    Envía un mensaje a todos los clientes conectados.
    Si se especifica 'remitente', no le envía el mensaje a ese cliente.
    """
    if not clientes_conectados:
        return
    
    destinatarios = clientes_conectados if not remitente else clientes_conectados - {remitente}
    
    if destinatarios:
        await asyncio.gather(
            *[cliente.send(mensaje) for cliente in destinatarios],
            return_exceptions=True
        )


async def manejar_cliente(websocket):
    """Maneja las conexiones de clientes WebSocket con soporte para múltiples usuarios."""
    ip_cliente, puerto_cliente = websocket.remote_address
    
    clientes_conectados.add(websocket)
    estadisticas["total_conexiones"] += 1
    
    logger.info(f"[CONECTADO] {ip_cliente}:{puerto_cliente} | Total clientes: {len(clientes_conectados)}")
    
    bienvenida = {
        "tipo": "bienvenida",
        "mensaje": f"Bienvenido al servidor, {ip_cliente}!",
        "clientes_conectados": len(clientes_conectados),
        "tu_ip": ip_cliente,
        "hora": datetime.now().isoformat()
    }
    await websocket.send(json.dumps(bienvenida))
    
    notificacion = json.dumps({
        "tipo": "nuevo_cliente",
        "ip": ip_cliente,
        "total_clientes": len(clientes_conectados)
    })
    await broadcast(notificacion, remitente=websocket)
    
    try:
        async for mensaje in websocket:
            estadisticas["mensajes_recibidos"] += 1
            
            logger.info(f"[{ip_cliente}] {mensaje[:100]}")
            
            # Simplemente hacer broadcast del mensaje a todos los demás clientes
            await broadcast(mensaje, remitente=websocket)
    
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"[DESCONECTADO] {ip_cliente}:{puerto_cliente}")
    
    finally:
        clientes_conectados.discard(websocket)
        
        logger.info(f"[DESCONECTADO] {ip_cliente}:{puerto_cliente} | Total clientes: {len(clientes_conectados)}")
        
        notificacion = json.dumps({
            "tipo": "cliente_desconectado",
            "ip": ip_cliente,
            "total_clientes": len(clientes_conectados)
        })
        await broadcast(notificacion)


async def iniciar_servidor(ip_publica):
    """Inicia el servidor WebSocket."""
    estadisticas["inicio_servidor"] = datetime.now()
    
    ip_local = obtener_ip_local()
    
    logger.info("\n" + "="*70)
    logger.info(f"Servidor WebSocket iniciado")
    logger.info(f"Hora de inicio: {estadisticas['inicio_servidor'].strftime('%Y-%m-%d %H:%M:%S')}")
    logger.info("="*70)
    logger.info(f"Desde tu red local: ws://{ip_local}:{PUERTO}")
    logger.info(f"Desde internet:     ws://{ip_publica}:{PUERTO}")
    logger.info("="*70 + "\n")
    
    async with websockets.serve(manejar_cliente, "0.0.0.0", PUERTO):
        try:
            await asyncio.Future()
        except KeyboardInterrupt:
            logger.info("\n" + "="*70)
            logger.info("Servidor detenido por el usuario")
            logger.info(f"Total conexiones: {estadisticas['total_conexiones']}")
            logger.info(f"Total mensajes: {estadisticas['mensajes_recibidos']}")
            logger.info("="*70)


def main():
    """Función principal - coordina todo el proceso."""
    logger.info("="*70)
    logger.info("Iniciando servidor...")
    logger.info("="*70)
    
    routers = buscar_routers()
    if not routers:
        logger.error("No se encontraron routers")
        return
    
    info_router = None
    for i, router in enumerate(routers, 1):
        info_router = probar_router(router, i)
        if info_router:
            break
    
    if not info_router:
        logger.error("No se encontró un router válido")
        return
    
    logger.info(f"\n✓ Router seleccionado: {info_router['nombre']}")
    logger.info(f"  Fabricante: {info_router['fabricante']}")
    logger.info(f"  Servicio: {info_router['servicio']}")
    
    ip_local = obtener_ip_local()
    logger.info(f"  IP local: {ip_local}")
    
    if not abrir_puerto(info_router, ip_local):
        logger.error("No se pudo abrir el puerto")
        return
    
    asyncio.run(iniciar_servidor(info_router['ip']))


if __name__ == "__main__":
    main()