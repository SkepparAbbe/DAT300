clear
selfScalability=0;
LineWidth=4;


dataset = 'MNIST';
platform = 'amazon';

prefix = sprintf('%s/%s', platform, dataset);

numberOfThreads = [1 2 3 4 5 10 15 20 25 30 35 40 45 50 55 60 65 70];

par(1).L=58 ; par(1).M=9; par(1).Marker = 'o'; par(1).LineStyle = '--'; par(1).accuracy=0.77; par(1).vaccuracy=NaN;
par(2).L=116 ; par(2).M=9; par(2).Marker = '*'; par(2).LineStyle = '--'; par(2).accuracy=0.85; par(2).vaccuracy=NaN;
par(3).L=230; par(3).M=9; par(3).Marker = '+'; par(3).LineStyle = '--'; par(3).accuracy=0.89; par(3).vaccuracy=NaN;

figure('Position', [10 10 600 600])
hold on

for i = 1:length(par)
    profile_lshdbscan(i,:) = sum(dlmread(sprintf('%s/LSHDBSCAN/benchmark_L_%d_M_%d.txt',prefix, par(i).L, par(i).M)),2);
    %text_lshdbscan{i} = sprintf('$\\mathtt{LSHDBSCAN,L=%d,M=%d,RI=%.2f}$', par(i).L, par(i).M, par(i).accuracy);
    
    text_lshdbscan{i} = sprintf('$\\mathtt{IPLSHDBSCAN:%d,%d,%.2f}$', par(i).L, par(i).M, par(i).accuracy);
    
    
    plot(numberOfThreads, profile_lshdbscan(i,:), 'LineWidth',LineWidth, 'MarkerSize',7, 'Marker', par(i).Marker, 'LineStyle', par(i).LineStyle)
    
end


myXlabel = xlabel('number of threads', 'Interpreter', 'latex');


xlim([0 70]);
xticks([1 10 20 30 40 50 60 70]);

mylegend = legend(...
    text_lshdbscan{1}, ...
    text_lshdbscan{2}, ...
    text_lshdbscan{3} ...
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
