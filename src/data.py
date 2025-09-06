import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import numpy as np
import shutil

# --- Configurações Iniciais ---
base_folder_brkga = 'output'
base_folder_dp = 'output_dp'
output_folder_plots = 'comparison_plots'

# Cria a pasta de saída principal para os gráficos, se não existir
os.makedirs(output_folder_plots, exist_ok=True)

# --- Identificação das Subpastas para Comparação ---
# Encontra as subpastas que existem tanto em 'output' quanto em 'output_dp'
try:
    subfolders_brkga = set(os.listdir(base_folder_brkga))
    subfolders_dp = set(os.listdir(base_folder_dp))
    # Filtra para manter apenas diretórios com nomes numéricos e os ordena
    common_subfolders = sorted([d for d in subfolders_brkga.intersection(subfolders_dp) if d.isdigit()], key=int)

    if not common_subfolders:
        print("Nenhuma subpasta numérica em comum encontrada entre 'output' e 'output_dp'.")
        print("Verifique se as pastas existem e contêm subpastas como '8', '16', etc.")
        exit()

    print(f"Subpastas encontradas para comparação: {', '.join(common_subfolders)}")

except FileNotFoundError as e:
    print(f"Erro: A pasta '{e.filename}' não foi encontrada.")
    print("Certifique-se de que as pastas base 'output' e 'output_dp' existem no mesmo diretório do script.")
    exit()


# --- Loop Principal para Análise e Visualização ---
for subfolder in common_subfolders:
    print(f"\n{'='*25} Processando Subpasta: {subfolder} {'='*25}")

    # Define a pasta de saída para os gráficos desta subpasta
    current_plot_folder = os.path.join(output_folder_plots, subfolder)

    # Cria a pasta de saída (ou a limpa se já existir)
    if os.path.exists(current_plot_folder):
        shutil.rmtree(current_plot_folder)
    os.makedirs(current_plot_folder)

    # Define os caminhos para os arquivos de objetivos e tempo
    objectives_file_brkga = os.path.join(base_folder_brkga, subfolder, 'objectives.txt')
    time_file_brkga = os.path.join(base_folder_brkga, subfolder, 'time.txt')
    objectives_file_dp = os.path.join(base_folder_dp, subfolder, 'objectives.txt')
    time_file_dp = os.path.join(base_folder_dp, subfolder, 'time.txt')

    try:
        # --- Leitura e Preparação dos Dados ---
        # Ler os dados do BRKGA padrão (objetivo e tempo)
        df_brkga_obj = pd.read_csv(objectives_file_brkga, sep=' ', header=None, usecols=[0], names=['objective_brkga'])
        df_brkga_time = pd.read_csv(time_file_brkga, header=None, names=['time_brkga'])

        # Ler os dados do BRKGA com DP (objetivo e tempo)
        df_dp_obj = pd.read_csv(objectives_file_dp, sep=' ', header=None, usecols=[0], names=['objective_dp'])
        df_dp_time = pd.read_csv(time_file_dp, header=None, names=['time_dp'])

        # Combinar todos os dados em um único DataFrame
        df_comparison = pd.concat([df_brkga_obj, df_dp_obj, df_brkga_time, df_dp_time], axis=1)
        df_comparison['instance'] = df_comparison.index + 1
        
        epsilon = 1e-9 # Para evitar divisão por zero

        # --- Análise Comparativa dos OBJETIVOS ---
        # Maior valor de objetivo é melhor
        df_comparison['winner_obj'] = 'Empate'
        df_comparison.loc[df_comparison['objective_dp'] > df_comparison['objective_brkga'], 'winner_obj'] = 'BRKGA_DP'
        df_comparison.loc[df_comparison['objective_brkga'] > df_comparison['objective_dp'], 'winner_obj'] = 'BRKGA'
        df_comparison['improvement_factor'] = df_comparison['objective_dp'] / (df_comparison['objective_brkga'] + epsilon)

        # --- Análise Comparativa dos TEMPOS ---
        # Menor tempo é melhor
        df_comparison['winner_time'] = 'Empate'
        df_comparison.loc[df_comparison['time_dp'] < df_comparison['time_brkga'], 'winner_time'] = 'BRKGA_DP'
        df_comparison.loc[df_comparison['time_brkga'] < df_comparison['time_dp'], 'winner_time'] = 'BRKGA'
        df_comparison['speedup_factor'] = df_comparison['time_brkga'] / (df_comparison['time_dp'] + epsilon)


        # --- Exibindo Resultados no Console ---
        print(f"--- Resumo da Comparação para a subpasta '{subfolder}' ---")
        print("\n[OBJETIVO] Contagem de vitórias (maior é melhor):")
        print(df_comparison['winner_obj'].value_counts())
        print("\n[TEMPO] Contagem de vitórias (menor é melhor):")
        print(df_comparison['winner_time'].value_counts())
        print("\n--- Estatísticas Descritivas (Valores da Função Objetivo) ---")
        print(df_comparison[['objective_brkga', 'objective_dp']].describe())
        print("\n--- Estatísticas Descritivas (Tempo de Execução em segundos) ---")
        print(df_comparison[['time_brkga', 'time_dp']].describe())
        print("\n--- Tabela Comparativa (Primeiras 15 instâncias) ---")
        print(df_comparison[['instance', 'objective_brkga', 'objective_dp', 'time_brkga', 'time_dp']].head(15).to_string())

        # --- Gerando Gráficos ---
        sns.set_style("whitegrid")

        # ===================================================================
        # GRÁFICOS PARA OS VALORES DA FUNÇÃO OBJETIVO
        # ===================================================================

        # 1. Gráfico de Barras Comparativo por Instância (Objetivo)
        plt.figure(figsize=(18, 8))
        df_melted_obj = df_comparison.melt(id_vars='instance', value_vars=['objective_brkga', 'objective_dp'],
                                           var_name='Abordagem', value_name='Valor Objetivo')
        sns.barplot(x='instance', y='Valor Objetivo', hue='Abordagem', data=df_melted_obj)
        plt.title(f'Comparação do Valor Objetivo por Instância (Subpasta: {subfolder})', fontsize=16)
        plt.xlabel('Instância', fontsize=12)
        plt.ylabel('Valor da Função Objetivo', fontsize=12)
        plt.xticks(rotation=45, ha='right')
        plt.tight_layout()
        plt.savefig(os.path.join(current_plot_folder, '01_objective_comparison_bar.png'))
        plt.close()

        # 2. Boxplot para Comparação da Distribuição (Objetivo)
        plt.figure(figsize=(10, 7))
        sns.boxplot(data=df_comparison[['objective_brkga', 'objective_dp']])
        plt.title(f'Distribuição dos Valores da Função Objetivo (Subpasta: {subfolder})', fontsize=16)
        plt.ylabel('Valor da Função Objetivo', fontsize=12)
        plt.xlabel('Abordagem', fontsize=12)
        plt.xticks(ticks=[0, 1], labels=['BRKGA Padrão', 'BRKGA com DP'])
        plt.savefig(os.path.join(current_plot_folder, '02_objective_boxplot.png'))
        plt.close()

        # 3. Gráfico de Dispersão (Scatter Plot) para Comparação Direta (Objetivo)
        plt.figure(figsize=(10, 10))
        sns.scatterplot(x='objective_brkga', y='objective_dp', data=df_comparison, hue='winner_obj', s=60, palette={'BRKGA_DP': 'green', 'BRKGA': 'red', 'Empate': 'blue'})
        max_val = max(df_comparison['objective_brkga'].max(), df_comparison['objective_dp'].max())
        min_val = min(df_comparison['objective_brkga'].min(), df_comparison['objective_dp'].min())
        plt.plot([min_val, max_val], [min_val, max_val], 'k--', lw=2, label='y=x (Empate)')
        plt.title(f'BRKGA com DP vs. BRKGA Padrão (Subpasta: {subfolder})', fontsize=16)
        plt.xlabel('Valor Objetivo (BRKGA Padrão)', fontsize=12)
        plt.ylabel('Valor Objetivo (BRKGA com DP)', fontsize=12)
        plt.legend(title='Melhor Resultado')
        plt.axis('equal')
        plt.grid(True)
        plt.savefig(os.path.join(current_plot_folder, '03_objective_scatterplot.png'))
        plt.close()

        # 4. Gráfico de Barras do Fator de Melhoria por Instância (Objetivo)
        plt.figure(figsize=(18, 8))
        sns.barplot(x='instance', y='improvement_factor', data=df_comparison, color='mediumseagreen')
        plt.axhline(y=1, color='r', linestyle='--', label='Empate (Fator = 1)')
        plt.title(f'Fator de Melhoria do Objetivo (DP vs. Padrão) por Instância (Subpasta: {subfolder})', fontsize=16)
        plt.xlabel('Instância', fontsize=12)
        plt.ylabel('Fator de Melhoria (objective_dp / objective_brkga)', fontsize=12)
        plt.xticks(rotation=45, ha='right')
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(current_plot_folder, '04_objective_improvement_factor.png'))
        plt.close()

        # ===================================================================
        # GRÁFICOS PARA O TEMPO DE EXECUÇÃO
        # ===================================================================

        # 5. Gráfico de Barras Comparativo por Instância (Tempo)
        plt.figure(figsize=(18, 8))
        df_melted_time = df_comparison.melt(id_vars='instance', value_vars=['time_brkga', 'time_dp'],
                                           var_name='Abordagem', value_name='Tempo (s)')
        sns.barplot(x='instance', y='Tempo (s)', hue='Abordagem', data=df_melted_time)
        plt.title(f'Comparação do Tempo de Execução por Instância (Subpasta: {subfolder})', fontsize=16)
        plt.xlabel('Instância', fontsize=12)
        plt.ylabel('Tempo de Execução (segundos)', fontsize=12)
        plt.xticks(rotation=45, ha='right')
        plt.tight_layout()
        plt.savefig(os.path.join(current_plot_folder, '05_time_comparison_bar.png'))
        plt.close()

        # 6. Boxplot para Comparação da Distribuição (Tempo)
        plt.figure(figsize=(10, 7))
        sns.boxplot(data=df_comparison[['time_brkga', 'time_dp']])
        plt.title(f'Distribuição dos Tempos de Execução (Subpasta: {subfolder})', fontsize=16)
        plt.ylabel('Tempo de Execução (segundos)', fontsize=12)
        plt.xlabel('Abordagem', fontsize=12)
        plt.xticks(ticks=[0, 1], labels=['BRKGA Padrão', 'BRKGA com DP'])
        plt.savefig(os.path.join(current_plot_folder, '06_time_boxplot.png'))
        plt.close()
        
        # 7. Gráfico de Barras do Fator de Speedup por Instância (Tempo)
        plt.figure(figsize=(18, 8))
        sns.barplot(x='instance', y='speedup_factor', data=df_comparison, color='skyblue')
        plt.axhline(y=1, color='r', linestyle='--', label='Empate (Speedup = 1)')
        plt.title(f'Fator de Speedup (Padrão vs. DP) por Instância (Subpasta: {subfolder})', fontsize=16)
        plt.xlabel('Instância', fontsize=12)
        plt.ylabel('Fator de Speedup (time_brkga / time_dp)', fontsize=12)
        plt.xticks(rotation=45, ha='right')
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(current_plot_folder, '07_time_speedup_factor.png'))
        plt.close()


        print(f"-> Gráficos para a subpasta '{subfolder}' salvos com sucesso em: '{current_plot_folder}'")

    except FileNotFoundError as e:
        print(f"!! Erro ao processar a subpasta '{subfolder}': Arquivo não encontrado -> {e.filename}.")
        print("   Verifique se os arquivos 'objectives.txt' e 'time.txt' existem nesta subpasta. Pulando para a próxima.")
    except Exception as e:
        print(f"!! Ocorreu um erro inesperado ao processar a subpasta '{subfolder}': {e}")

print(f"\n{'='*20} Processo de Geração de Gráficos Concluído {'='*20}\n")