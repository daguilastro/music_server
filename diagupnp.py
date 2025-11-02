import upnpclient
import logging

logging.basicConfig(level=logging.WARNING)  # Menos debug

print("Buscando dispositivos UPnP...\n")
dispositivos = upnpclient.discover()

for device in dispositivos:
    print(f"{'='*70}")
    print(f"Dispositivo: {device.friendly_name}")
    print(f"Tipo: {device.device_type}")
    print(f"Fabricante: {device.manufacturer}")
    
    print(f"\n✓ Servicios disponibles ({len(device.services)}):")
    
    for service in device.services:
        # Extraer el nombre corto del servicio
        service_name = service.service_id.split(':')[-1]
        print(f"  - {service_name}")
        print(f"    ID completo: {service.service_id}")
        
        # Intentar listar acciones disponibles
        try:
            if hasattr(service, 'actions'):
                actions = [a for a in service.actions]
                if actions:
                    print(f"    Acciones: {', '.join([a.name for a in actions[:3]])}")
        except:
            pass
    
    print()