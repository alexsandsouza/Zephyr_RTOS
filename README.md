# Laboratório Zephyr RTOS + QEMU

Laboratório acadêmico de Sistemas de Tempo Real (PPGEE/UFAM), desenvolvido para execução **sem placa física**, com o emulador QEMU.

**Responsável:** Alexsand Farias de Souza  
**Disciplina:** PGENE537 - Sistemas de Tempo Real  
**Professor:** Lucas Carvalho Cordeiro

> Projeto didático independente. Logs emulados não constituem prova de comportamento temporal em hardware real ou medição de WCET.

## Roteiro de referência da UFAM

Este repositório é baseado no documento fornecido pelo aluno **“Roteiro de Laboratório: Configuração do Ambiente Zephyr RTOS - Atividade 1.1”** (PPGEE/UFAM, PGENE537, 2026.2), professor **Lucas Carvalho Cordeiro**, monitor **Janislley Oliveira de Sousa**.

O roteiro original, de quatro páginas, prescreve:
1. Dependências e ambiente Ubuntu 24.04, Python, `west`, módulos e Zephyr SDK (seção 5).
2. VS Code e terminal integrado (seção 6).
3. Hello World na XIAO nRF52840, compilação e firmware UF2 (seção 7).
4. Hello World na ESP32-S3 DevKitC, gravação por `west flash` (seção 8).
5. Coleta de evidências e resolução de problemas (seções 9–10).

**Adaptação didática, não presente no roteiro:** por ausência das placas físicas, substituímos compilação/gravação física por `qemu_x86` + `west build -t run`. Os laboratórios de produtor-consumidor e escalonamento são extensões autorais para prática de threads, sincronização e análise temporal. Os experimentos em QEMU não validam prazos em hardware real.

**Nota:** o PDF original foi fornecido na conversa, mas não está armazenado como arquivo neste repositório.

## Objetivos
1. Executar o Zephyr RTOS em uma placa virtual QEMU.
2. Experimentar comunicação por filas entre threads.
3. Preservar identificador e timestamp original dos jobs.
4. Verificar cumprimento de deadlines e detectar perdas de mensagens.
5. Comparar cenários de execução preemptiva e cooperativa.

## Estrutura

```text
labs/
  01-hello-world/
    CMakeLists.txt
    prj.conf
    src/main.c
  02-produtor-consumidor/
    CMakeLists.txt
    prj.conf
    src/main.c
  03-escalonamento/
    CMakeLists.txt
    prj.conf
    src/main.c
```

## Preparação do Ubuntu 24.04

Consulte também [Getting Started oficial](https://docs.zephyrproject.org/latest/develop/getting_started/index.html). No terminal:

```bash
sudo apt update
sudo apt install --no-install-recommends \
  git cmake ninja-build gperf ccache dfu-util device-tree-compiler \
  wget python3-dev python3-venv python3-tk xz-utils file make \
  gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
mkdir -p ~/zephyrproject
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
python -m pip install --upgrade pip
pip install west
west init ~/zephyrproject
cd ~/zephyrproject
west update
west packages pip --install
west zephyr-export
cd zephyr
west sdk install
```

Abra um novo terminal? Ative novamente o ambiente com `source ~/zephyrproject/.venv/bin/activate`.

## Baixar este laboratório

```bash
cd ~/zephyrproject
git clone https://github.com/alexsandsouza/Zephyr_RTOS.git
```

## Executar

No terminal com o ambiente ativado:

```bash
cd ~/zephyrproject
west build -p always -b qemu_x86 Zephyr_RTOS/labs/01-hello-world -d build/lab01
west build -d build/lab01 -t run

west build -p always -b qemu_x86 Zephyr_RTOS/labs/02-produtor-consumidor -d build/lab02
west build -d build/lab02 -t run

west build -p always -b qemu_x86 Zephyr_RTOS/labs/03-escalonamento -d build/lab03
west build -d build/lab03 -t run
```

Para sair do QEMU: **Ctrl+A**, depois **X**.

## Experimento 02: produtor-consumidor

O produtor emite 30 leituras. O consumidor avalia o limiar e uma thread atuadora registra o tempo de resposta **da liberação original até a atuação**. Mensagens com o mesmo `job_id` mantêm o `release_ms` original. As filas têm capacidade finita; inserções não bloqueantes que falham são contadas como descarte.

**Importante:** o intervalo aproximado de 20 ms do produtor usa `k_sleep()`, não um temporizador de liberações absolutas; há jitter e deriva. O experimento demonstra instrumentação, não periodicidade rigorosa.

## Experimento 03: cooperação vs. preempção

No arquivo `labs/03-escalonamento/src/main.c`, altere `SCENARIO_COOPERATIVE` entre `0` e `1`, recompile e compare os logs. No cenário cooperativo, uma thread que ocupa a CPU sem ceder pode atrasar outra, ainda que esta tenha prioridade relativamente mais alta no grupo preemptivo. O ensaio usa ocupação artificial da CPU para facilitar observação; resultados dependem da emulação.

## Evidências sugeridas

- Versões de Ubuntu, `west` e Zephyr.
- Saída de build e inicialização do QEMU.
- Para cada job: `job_id`, release, conclusão, resposta, deadline.
- Totais produzidos, concluídos, descartados e atrasados.
- Diferenças entre cenários preemptivo e cooperativo.

## Referências

- [Zephyr - Getting Started](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
- [Zephyr - Hello World](https://docs.zephyrproject.org/latest/samples/hello_world/README.html)
- [Zephyr - Message Queue](https://docs.zephyrproject.org/latest/samples/kernel/msg_queue/README.html)
- [Zephyr - Synchronization](https://docs.zephyrproject.org/latest/samples/synchronization/README.html)

## Licença

Conteúdo educacional inicial disponibilizado para estudo. Consulte as licenças próprias do Zephyr e dependências.
