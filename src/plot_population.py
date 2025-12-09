import os
import numpy as np
import matplotlib.pyplot as plt

def plot_population_cloud(file_path):
    """
    Lê um arquivo population.txt e gera um gráfico de dispersão (nuvem).
    Salva o gráfico na mesma pasta do arquivo original.
    """
    try:
        # Carrega os dados
        # Cada linha é uma geração, cada coluna é um indivíduo
        data = np.loadtxt(file_path)
        
        # Verifica se o arquivo não está vazio ou mal formatado
        if data.ndim == 1:
            # Caso tenha apenas 1 geração, transforma em 2D para a lógica funcionar
            data = data.reshape(1, -1)
            
        if data.size == 0:
            print(f"Aviso: Arquivo vazio ignorado -> {file_path}")
            return

        num_generations, pop_size = data.shape
        
        # Parâmetros baseados no seu código C++
        # pe = 0.25, então os primeiros 25% (ordenados) são elite
        pe = 0.05
        num_elite = int(pop_size * pe)

        # Separação dos dados
        # O BRKGA mantém a população ordenada, então os primeiros índices são os melhores
        elite_data = data[:, :num_elite]      # Primeiras colunas (Melhores/Elite)
        non_elite_data = data[:, num_elite:]  # Restante das colunas
        
        # Criação do Eixo X (Gerações)
        # Repetimos o número da geração para cada indivíduo para poder plotar o scatter
        generations = np.arange(num_generations)
        
        # Prepara arrays "achatados" (flatten) para plotagem em massa
        # X para elite: repete o índice da geração para cada membro da elite
        x_elite = np.repeat(generations, elite_data.shape[1])
        y_elite = elite_data.flatten()
        
        # X para não-elite
        x_non_elite = np.repeat(generations, non_elite_data.shape[1])
        y_non_elite = non_elite_data.flatten()

        # --- Plotagem ---
        plt.figure(figsize=(12, 6))
        
        # Plota Não-Elite (Azul) primeiro para ficar "atrás" visualmente se sobrepor
        # Alpha diminui a opacidade para ver a densidade da nuvem
        plt.scatter(x_non_elite, y_non_elite, color='blue', s=2, alpha=0.3, label='Não-Elite')
        
        # Plota Elite (Vermelho)
        plt.scatter(x_elite, y_elite, color='red', s=2, alpha=0.5, label='Elite')

        # Configurações do Gráfico
        plt.title(f'Evolução da População - Throughput\nArquivo: {file_path}')
        plt.xlabel('Geração')
        plt.ylabel('Throughput (Valor da FO)')
        plt.legend(markerscale=5) # Aumenta o tamanho do ponto na legenda para ficar visível
        plt.grid(True, linestyle='--', alpha=0.5)
        
        # Salva o gráfico
        output_filename = os.path.join(os.path.dirname(file_path), 'population_plot.png')
        plt.savefig(output_filename, dpi=150)
        plt.close() # Fecha a figura para liberar memória
        
        print(f"Gráfico gerado com sucesso: {output_filename}")

    except Exception as e:
        print(f"Erro ao processar {file_path}: {e}")

def main():
    # Diretório raiz onde o script está rodando (procurará em todas as subpastas)
    root_dir = "."
    target_file = "population.txt"

    print(f"Iniciando busca por arquivos '{target_file}' em '{os.path.abspath(root_dir)}'...\n")

    found = False
    # Walk percorre todas as subpastas
    for dirpath, dirnames, filenames in os.walk(root_dir):
        if target_file in filenames:
            found = True
            full_path = os.path.join(dirpath, target_file)
            plot_population_cloud(full_path)
            
    if not found:
        print("Nenhum arquivo 'population.txt' foi encontrado.")

if __name__ == "__main__":
    main()