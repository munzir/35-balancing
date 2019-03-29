data_files = {'hardware-adrc', ...
    'hardware-adrc-beta128-03011716', ...
    'hardware-adrc-good-com-03011525', ...
    'hardware-adrc-good-com-03011603', ...
    'hardware-lqr-only', ...
    'hardware-lqr-only-beta64-03011747', ...
    'hardware-lqr-only-beta128-03011711', ...
    'hardware-lqr-only-good-com-03011514', ...
    'hardware-lqr-only-good-com-03011557'};

n = length(data_files);
means = zeros(n+1,1); stds = zeros(n+1,1); medians = zeros(n+1,1); 
modes = zeros(n+1,1); mins = zeros(n+1,1); maxes = zeros(n+1,1);
dts = [];
for i=1:n
    data_bal = dlmread(data_files{i});
    time = data_bal(:,1);
    dt = time(2:end) - time(1:end-1);
    dts = [dts; dt];
    means(i) = mean(dt);
    stds(i) = std(dt);
    medians(i) = median(dt);
    modes(i) = mode(round(dt,3));
    mins(i) = min(dt);
    maxes(i) = max(dt);
end

means(end) = mean(dts);
stds(end) = std(dts);
medians(end) = median(dts);
modes(end) = mode(round(dts,3));
mins(end) = min(dts);
maxes(end) = max(dts);

display(['mean:   ' num2str(means(end))]);
display(['std:    ' num2str(stds(end))]);
display(['median: ' num2str(medians(end))]);
display(['mode:   ' num2str(modes(end))]);
display(['min:    ' num2str(mins(end))]);
display(['max:    ' num2str(maxes(end))]);

figure;
subplot(2,3,1);
bar(means);
title('Mean')

subplot(2,3,2);
bar(stds);
title('Std')

subplot(2,3,3);
bar(medians);
title('Median')

subplot(2,3,4);
bar(modes);
title('Mode')

subplot(2,3,5);
bar(mins);
title('Min')

subplot(2,3,6);
bar(maxes);
title('Max')