clear
selfScalability=0;
LineWidth=4;


dataset = 'geolife'; %household, geolife, bremen, tera_67M
platform = 'amazon'; %ody, amazon

prefix = sprintf('%s/%s', platform, dataset);

numberOfThreads = [1 2 3 4 5 10 15 20 25 30 35 40 45 50 55 60 65 70];

profile_hpdbscan  = dlmread(sprintf('%s/HPDBSCAN/benchmark_hpdbscan.txt',prefix));
profile_pdsdbscan = dlmread(sprintf('%s/PDSDBSCAN/benchmark_pdsdbscan.txt',prefix));
profile_tedbscan = dlmread(sprintf('%s/TEDBSCAN/benchmark_tedbscan.txt',prefix));
idx = ~isnan(profile_tedbscan);

if strcmp(dataset, 'household')
    par(1).L=5 ; par(1).M=5; par(1).Marker = 'o'; par(1).LineStyle = '--'; par(1).accuracy=0.92; par(1).vaccuracy=0.99;
    par(2).L=10 ; par(2).M=5; par(2).Marker = '*'; par(2).LineStyle = '--'; par(2).accuracy=0.94; par(2).vaccuracy=NaN;
    par(3).L=20; par(3).M=5; par(3).Marker = '+'; par(3).LineStyle = '--'; par(3).accuracy=0.95; par(3).vaccuracy=NaN;
elseif strcmp(dataset, 'tera_67M')
    par(1).L=5	 ; par(1).M=5; par(1).Marker = 'o'; par(1).LineStyle = '--'; par(1).accuracy=0.98; par(1).vaccuracy=NaN;
    par(2).L=10 ; par(2).M=5; par(2).Marker = '*'; par(2).LineStyle = '--'; par(2).accuracy=0.99; par(2).vaccuracy=NaN;
    par(3).L=20; par(3).M=5; par(3).Marker = '+'; par(3).LineStyle = '--'; par(3).accuracy=1; par(3).vaccuracy=NaN;
elseif strcmp(dataset, 'geolife')
    par(1).L=5 ; par(1).M=2; par(1).Marker = 'o'; par(1).LineStyle = '--'; par(1).accuracy=0.8; par(1).vaccuracy=0.99;
    par(2).L=10; par(2).M=2; par(2).Marker = '*'; par(2).LineStyle = '--'; par(2).accuracy=0.85; par(2).vaccuracy=1;
    par(3).L=20; par(3).M=2; par(3).Marker = '+'; par(3).LineStyle = '--'; par(3).accuracy=0.89; par(3).vaccuracy=NaN;
end

figure('Position', [10 10 600 700])
hold on

for i = 1:length(par)
    profile_lshdbscan(i,:) = sum(dlmread(sprintf('%s/LSHDBSCAN/benchmark_L_%d_M_%d.txt',prefix, par(i).L, par(i).M)),2);
    %text_lshdbscan{i} = sprintf('$\\mathtt{LSHDBSCAN,L=%d,M=%d,RI=%.2f}$', par(i).L, par(i).M, par(i).accuracy);
    
    text_lshdbscan{i} = sprintf('$\\mathtt{IPLSHDBSCAN:%d,%d,%.2f}$', par(i).L, par(i).M, par(i).accuracy);
    
    
    if selfScalability==1
        plot(numberOfThreads, profile_lshdbscan(i,1)./profile_lshdbscan(i,:), 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', par(i).Marker, 'LineStyle', par(i).LineStyle)
    else
        plot(numberOfThreads, profile_lshdbscan(i,:), 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', par(i).Marker, 'LineStyle', par(i).LineStyle)
    end
end

if selfScalability==1
    plot(numberOfThreads(idx), profile_tedbscan(1)./profile_tedbscan(idx), 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', '^', 'LineStyle', '--')
    plot(numberOfThreads, profile_hpdbscan(1)./profile_hpdbscan, 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', 'square', 'LineStyle', '--')
    plot(numberOfThreads, profile_pdsdbscan(1)./profile_pdsdbscan, 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', 'diamond', 'LineStyle', '--')
else
    plot(numberOfThreads(idx), profile_tedbscan(idx), 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', '^', 'LineStyle', '--')
    plot(numberOfThreads, profile_hpdbscan, 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', 'square', 'LineStyle', '--')
    plot(numberOfThreads, profile_pdsdbscan, 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', 'diamond', 'LineStyle', '--')
end

myXlabel = xlabel('number of threads', 'Interpreter', 'latex');


xlim([0 70]);
xticks([1 10 20 30 40 50 60 70]);

mylegend = legend(...
    text_lshdbscan{1}, ...
    text_lshdbscan{2}, ...
    text_lshdbscan{3}, ...
    '${\mathtt{TEDBSCAN}}$',...
    '${\mathtt{HPDBSCAN}}$',...
    '${\mathtt{PDSDBSCAN}}$'...
    );

mylegend.Interpreter = 'latex';
mylegend.Box='off';

mylegend.FontSize = 25;

xlim([0.5 35]);
xticks([1 10 20 30 35]);
xticklabels({1, 10, 20', 30, 36});

if selfScalability == 0
    set(gca, 'YScale', 'log')
    myYlabel = ylabel('completion time (seconds)', 'Interpreter', 'latex');
else
    myYlabel = ylabel('self-scalability', 'Interpreter', 'latex');
end

set(gca,'fontsize',25);