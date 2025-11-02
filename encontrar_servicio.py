import upnpclient
import logging

logging.basicConfig(level=logging.WARNING)

print("Buscando router...\n")
dispositivos = upnpclient.discover()

for device in dispositivos:
    if "InternetGatewayDevice" in device.device_type:
        print(f"Router: {device.friendly_name}")
        print(f"\nServicios (nombres de atributo):")
        
        # Listar todos los atributos
        for service in device.services:
            service_id = service.service_id
            # Extraer el nombre corto
            nombre_corto = service_id.split(':')[-1]
            print(f"  ✓ {nombre_corto}")
        
        print(f"\nIntentando acceder a cada uno:")
        for service in device.services:
            nombre_corto = service.service_id.split(':')[-1]
            try:
                servicio = getattr(device, nombre_corto)
                print(f"  ✓ {nombre_corto} -> FUNCIONA")
                print(f"    Acciones disponibles:")
                for action in servicio.actions:
                    print(f"      - {action.name}")
            except AttributeError as e:
                print(f"  ✗ {nombre_corto} -> {e}")