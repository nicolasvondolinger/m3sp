import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import numpy as np

# --- Configurações Iniciais ---
# Define as pastas de entrada e a pasta de saída para os gráficos
base_folder_brkga = 'output'
base_folder_dp = 'output_dp'
output_folder_plots = 'comparison_plots'

# Cria a pasta de saída para os gráficos, se não existir
os.makedirs(output_folder_plots, exist_ok=True)

# Define os caminhos para os arquivos de objetivos
objectives_file_brkga = os.path.join(base_folder_brkga, 'objectives_brkga.txt')
objectives_file_dp = os.path.join(base_folder_dp, 'objectives_brkga.txt')

# --- Análise e Visualização dos Dados ---
try:
    # --- Leitura e Preparação dos Dados ---
    # Verificar se os arquivos existem antes de tentar lê-los
    if not os.path.exists(objectives_file_brkga):
        raise FileNotFoundError(f"Arquivo não encontrado: {objectives_file_brkga}")
    if not os.path.exists(objectives_file_dp):
        raise FileNotFoundError(f"Arquivo não encontrado: {objectives_file_dp}")

    # Ler os dados do BRKGA padrão
    df_brkga = pd.read_csv(objectives_file_brkga, sep=' ', header=None, names=['objective_brkga', 'time_brkga'])

    # Ler os dados do BRKGA com DP
    df_dp = pd.read_csv(objectives_file_dp, sep=' ', header=None, names=['objective_dp', 'time_dp'])

    # Combinar os DataFrames para comparação
    df_comparison = pd.concat([df_brkga['objective_brkga'], df_dp['objective_dp']], axis=1)
    df_comparison['instance'] = df_comparison.index + 1

    # --- Análise Comparativa ---
    # Determinar qual abordagem foi melhor para cada instância (maior valor objetivo é melhor)
    df_comparison['winner'] = 'Empate'
    df_comparison.loc[df_comparison['objective_dp'] > df_comparison['objective_brkga'], 'winner'] = 'BRKGA_DP'
    df_comparison.loc[df_comparison['objective_brkga'] > df_comparison['objective_dp'], 'winner'] = 'BRKGA'

    # Calcular a melhoria percentual do DP sobre o BRKGA
    # Adicionado um pequeno epsilon para evitar divisão por zero, caso o valor do objetivo seja 0
    epsilon = 1e-9
    df_comparison['improvement_dp_vs_brkga'] = ((df_comparison['objective_dp'] - df_comparison['objective_brkga']) / (df_comparison['objective_brkga'] + epsilon)) * 100


    # --- Exibindo Resultados no Console ---
    print("--- Resumo da Comparação ---")
    print("Contagem de vitórias por abordagem:")
    print(df_comparison['winner'].value_counts())
    print("\n--- Estatísticas Descritivas (Valores da Função Objetivo) ---")
    print(df_comparison[['objective_brkga', 'objective_dp']].describe())
    print("\n--- Estatísticas da Melhoria Percentual (DP vs BRKGA) ---")
    print(df_comparison['improvement_dp_vs_brkga'].describe())
    print("\n--- Tabela Comparativa (Primeiras 15 instâncias) ---")
    print(df_comparison.head(15).to_string())


    # --- Gerando Gráficos ---
    sns.set_style("whitegrid")

    # 1. Gráfico de Barras Comparativo por Instância
    plt.figure(figsize=(18, 8))
    # Prepara os dados para o gráfico de barras
    df_melted = df_comparison.melt(id_vars='instance', value_vars=['objective_brkga', 'objective_dp'],
                                   var_name='Abordagem', value_name='Valor Objetivo')
    sns.barplot(x='instance', y='Valor Objetivo', hue='Abordagem', data=df_melted)
    plt.title('Comparação do Valor Objetivo por Instância', fontsize=16)
    plt.xlabel('Instância', fontsize=12)
    plt.ylabel('Valor da Função Objetivo', fontsize=12)
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()
    plt.savefig(os.path.join(output_folder_plots, 'comparison_bar_chart.png'))
    plt.close()

    # 2. Boxplot para Comparação da Distribuição
    plt.figure(figsize=(10, 7))
    sns.boxplot(data=df_comparison[['objective_brkga', 'objective_dp']])
    plt.title('Distribuição dos Valores da Função Objetivo', fontsize=16)
    plt.ylabel('Valor da Função Objetivo', fontsize=12)
    plt.xlabel('Abordagem', fontsize=12)
    plt.xticks(ticks=[0, 1], labels=['BRKGA Padrão', 'BRKGA com DP'])
    plt.savefig(os.path.join(output_folder_plots, 'comparison_boxplot.png'))
    plt.close()

    # 3. Gráfico de Dispersão (Scatter Plot) para Comparação Direta
    plt.figure(figsize=(10, 10))
    sns.scatterplot(x='objective_brkga', y='objective_dp', data=df_comparison, hue='winner', s=60)
    # Adiciona uma linha y=x para referência visual
    max_val = max(df_comparison['objective_brkga'].max(), df_comparison['objective_dp'].max())
    min_val = min(df_comparison['objective_brkga'].min(), df_comparison['objective_dp'].min())
    plt.plot([min_val, max_val], [min_val, max_val], 'k--', lw=2, label='y=x (Empate)')
    plt.title('BRKGA com DP vs. BRKGA Padrão', fontsize=16)
    plt.xlabel('Valor Objetivo (BRKGA Padrão)', fontsize=12)
    plt.ylabel('Valor Objetivo (BRKGA com DP)', fontsize=12)
    plt.legend(title='Melhor Resultado')
    plt.axis('equal') # Garante que a escala dos eixos seja a mesma
    plt.grid(True)
    plt.savefig(os.path.join(output_folder_plots, 'comparison_scatterplot.png'))
    plt.close()

    # --- Cálculo do Fator de Melhoria ---
    # Adicionado um pequeno epsilon para evitar divisão por zero, caso o valor do objetivo seja 0
    epsilon = 1e-9
    df_comparison['improvement_factor'] = df_comparison['objective_dp'] / (df_comparison['objective_brkga'] + epsilon)


    # --- Gerando o novo Gráfico de Fator de Melhoria ---
    # 4. Gráfico de Barras do Fator de Melhoria por Instância
    plt.figure(figsize=(18, 8))
    sns.barplot(x='instance', y='improvement_factor', data=df_comparison, color='c')
    # Adiciona uma linha horizontal em y=1 para marcar o ponto de empate
    plt.axhline(y=1, color='r', linestyle='--', label='Empate (Fator = 1)')
    plt.title('Fator de Melhoria (DP vs. Padrão) por Instância', fontsize=16)
    plt.xlabel('Instância', fontsize=12)
    plt.ylabel('Fator de Melhoria (objective_dp / objective_brkga)', fontsize=12)
    plt.xticks(rotation=45, ha='right')
    plt.legend()
    plt.tight_layout()
    # Salva o novo gráfico na pasta de plots
    new_plot_path = os.path.join(output_folder_plots, 'improvement_factor_plot.png')
    plt.savefig(new_plot_path)
    plt.close()

    print(f"\nNovo gráfico de fator de melhoria salvo em: '{new_plot_path}'")
    print("\n--- Tabela com Fator de Melhoria (Primeiras 15 instâncias) ---")
    print(df_comparison[['instance', 'objective_brkga', 'objective_dp', 'improvement_factor']].head(15).to_string())

    print(f"\nGráficos salvos na pasta: '{output_folder_plots}'")

except FileNotFoundError as e:
    print(f"Erro: {e}. Certifique-se de que ambas as pastas 'output' e 'output_dp' existem e contêm os arquivos 'objectives_brkga.txt'.")
except Exception as e:
    print(f"Ocorreu um erro inesperado ao processar os arquivos: {e}")