import { spawn } from "child_process";

const windows = process.platform === 'win32';

export function which(exe: string): Promise<string | null> {

  if (windows) {
    return new Promise<string | null>((resolve) => {
      const proc = spawn('where', [exe]);
      let stdout = '';
      proc.stdout.on('data', (data: string) => {
        stdout += data;
      });
      proc.on('close', (code: number) => {
        if (code === 0) {
          resolve(stdout.split('\n')[0] || null);
        } else {
          resolve(null);
        }
      });
    });
  }

  return new Promise<string | null>((resolve) => {
    const proc = spawn('which', [exe]);
    let stdout = '';
    proc.stdout.on('data', (data: string) => {
      stdout += data;
    });
    proc.on('close', (code: number) => {
      const path = stdout.trim();
      if (code === 0 && path.length > 0) {
        resolve(path);
      } else {
        resolve(null);
      }
    })
  });
}
