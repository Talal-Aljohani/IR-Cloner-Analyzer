% plot_capture.m — plot a raw ADC dump from the IR Cloner (time signal + spectrum).
% 1) Arduino Serial Monitor (115200): send  r  (after a capture) or  x  (after an experiment)
% 2) Copy everything from the line "RAW fs=..." (or "EXP fs=...") to "END ..." into raw.txt next to this file
% 3) Run this script. No toolbox needed.

lines = strtrim(splitlines(fileread('raw.txt')));
h = find(startsWith(lines, {'RAW', 'EXP'}), 1);
e = find(startsWith(lines, 'END'), 1);
tok = regexp(lines{h}, '(\w+) fs=([\d.]+) n=(\d+) trig=(\d+)', 'tokens', 'once');
kind = tok{1};
fs = str2double(tok{2});
trig = str2double(tok{4});
x = str2double(lines(h + 1:e - 1));

t = (0:numel(x) - 1) / fs * 1e3;
subplot(2, 1, 1);
plot(t, x);
grid on;
xlabel('t (ms)');
ylabel('ADC counts');
title(sprintf('%s samples, fs = %.1f kSa/s', kind, fs / 1e3));

% Same FFT as the firmware: start 0.2 ms after the trigger, mean removed, Hann window.
if strcmp(kind, 'EXP')
    s0 = 1;
    N = 2^floor(log2(numel(x)));
else
    s0 = trig + round(200e-6 * fs) + 1;
    N = min(1024, 2^floor(log2(numel(x) - s0 + 1)));
end
seg = x(s0:s0 + N - 1);
w = 0.5 - 0.5 * cos(2 * pi * (0:N - 1)' / (N - 1));
X = abs(fft((seg - mean(seg)) .* w)) * 4 / N;
f = (0:N / 2 - 1) * fs / N;

subplot(2, 1, 2);
plot(f / 1e3, 20 * log10(X(1:N / 2) + eps));
grid on;
xlabel('f (kHz)');
ylabel('amplitude (dB counts)');
[~, k] = max(X(3:N / 2));
k = k + 2;
title(sprintf('Peak %.2f kHz,  N = %d,  \\Deltaf = fs/N = %.0f Hz', f(k) / 1e3, N, fs / N));
