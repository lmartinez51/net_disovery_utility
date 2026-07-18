import urllib.request
import sys

def fetch_scpd(ip, scpd_path):
    url = f"http://{ip}:7676{scpd_path}"
    print(f"Fetching: {url}")
    try:
        with urllib.request.urlopen(url, timeout=5) as response:
            xml = response.read().decode('utf-8')
            print("\n" + "="*50)
            print(f"SCPD for {scpd_path}:")
            print("="*50)
            print(xml)
            print("="*50 + "\n")
    except Exception as e:
        print(f"Failed to fetch {url}: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python fetch_scpds.py <TV_IP>")
        sys.exit(1)
        
    tv_ip = sys.argv[1]
    
    # Common SCPD paths for Samsung TVs
    scpds = [
        "/smp_4_",  # Usually RenderingControl
        "/smp_22_", # Usually ConnectionManager
        "/smp_24_", # Usually AVTransport
    ]
    
    for path in scpds:
        fetch_scpd(tv_ip, path)
