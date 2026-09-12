import asyncio
import cv2
import numpy as np
import websockets
import pyautogui

screen_size = pyautogui.size();
fps = 20.0;
frame_delay = 1/fps

print ("Recording started... Press 'q' to stop")

async def video_handler(websocket):
    client_ip, client_port = websocket.remote_address
    print(f"ESP32-CAM connected from: {client_ip} : {client_port}")
    
    try:
        while True:

            start_time = asyncio.get_event_loop().time()
            img = pyautogui.screenshot()

            frame = np.array(img)

            corrected_frame = cv2.cvtColor(frame,cv2.COLOR_RGB2BGR)

            resized_img = cv2.resize(corrected_frame,(320,240), fx = 0, fy = 0, interpolation=cv2.INTER_AREA)

            success, encoded_img = cv2.imencode('.jpg', resized_img,[int(cv2.IMWRITE_JPEG_QUALITY),90])

            if success:
                await websocket.send(encoded_img.tobytes())
            elapsed_time = asyncio.get_event_loop().time() - start_time

            sleep_time = max(0, frame_delay - elapsed_time)

            await asyncio.sleep(sleep_time)
            
        
                
    except websockets.exceptions.ConnectionClosed:
        print("Client {client_ip} disconnected.")

    except Exception as e:
        print(f"Error Occured: : {e}")
    finally:
        cv2.destroyAllWindows()

async def main():
    # Listen on port 82 across all local network interfaces
    port = 82
    print(f"Python WebSocket Server started. Listening on port {port}...")
    
    async with websockets.serve(video_handler, "0.0.0.0", port):
        await asyncio.Future()  # Keep the server running forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped manually.")
