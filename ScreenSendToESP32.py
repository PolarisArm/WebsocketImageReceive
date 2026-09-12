import asyncio
import cv2
import numpy as np
import websockets
import pyautogui
import mss


screen_size = pyautogui.size()
fps = 15.0
frame_delay = 1/fps

print ("Recording started... Press 'q' to stop")

async def video_handler(websocket):
    client_ip, client_port = websocket.remote_address
    print(f"ESP32-CAM connected from: {client_ip} : {client_port}")

    sct = mss.MSS()
    monitor = sct.monitors[1]
    
    try:
        while True:

            start_time = asyncio.get_event_loop().time()
            img = sct.grab(monitor) #pyautogui.screenshot()

            frame = np.array(img)

            #corrected_frame = cv2.cvtColor(frame,cv2.COLOR_RGB2BGR)

            resized_img = cv2.resize(frame,(320,240), fx = 0, fy = 0, interpolation=cv2.INTER_AREA)
            kernel = np.array([[0,-1,0],
                              [-1,5,-1],
                              [0,-1,0]])
            sharpImg = cv2.filter2D(resized_img, -1, kernel)
            

            success, encoded_img = cv2.imencode('.jpg', sharpImg,[int(cv2.IMWRITE_JPEG_QUALITY),85])

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
