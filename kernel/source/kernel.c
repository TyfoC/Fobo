#include <kernel.h>

extern VOID KernelMain(KERNEL_INIT_DATA* initData) {
	CopyMemory(KernelGDT, initData->GDTPointer, sizeof(GDT_ENTRY) * KERNEL_GDT_ENTRIES_COUNT);
	SetCursorPosition((STRING_POSITION){ 16, 0 });

	//	PMM
	MEMORY_REGION_DESCRIPTOR* memMap = (MEMORY_REGION_DESCRIPTOR*)initData->MemoryMapAddress;
	MEMORY_REGION_DESCRIPTOR extraMemRegs[] = {
		{ KERNEL_BASE_ADDRESS, KERNEL_SIZE, MEMORY_REGION_TYPE_RESERVED, 0 },	//	kernel
		{ 0, 0x500, MEMORY_REGION_TYPE_RESERVED, 0 },							//	BDA
		{ 0x80000, 0x80000, MEMORY_REGION_TYPE_RESERVED, 0 },					//	EBDA & other data
		{ 0xF00000, 0x100000, MEMORY_REGION_TYPE_RESERVED, 0 }					//	ISA hole
	};

	SIZE_T memMapLen = TransformBIOSMemoryMap(
		(ADDRESS_RANGE_DESCRIPTOR*)initData->MemoryMapAddress,
		(MEMORY_REGION_DESCRIPTOR*)initData->MemoryMapAddress,
		initData->MemoryMapLength
	);

	memMapLen = RemoveMemoryRegions(
		memMap, extraMemRegs, memMapLen,
		sizeof(extraMemRegs) / sizeof(extraMemRegs[0])
	);

	InitPMM(memMap, memMapLen);
	memMapLen = GetMemoryMapLength();

	for (SIZE_T i = 0; i < memMapLen; i++) {
		PrintFormatted(
			"%u) Addr: 0x%xu, Size: 0x%xu, Type: 0x%xu, PgCnt: 0x%xu\r\n", BIOS_COLOR_YELLOW,
			i + 1, memMap[i].Address, memMap[i].Size, memMap[i].Type, memMap[i].Length
		);
	}

	//	GDT, KernelCodeSegValue, KernelDataSegValue
	InitGDTRegister(KernelGDT, KERNEL_GDT_ENTRIES_COUNT, &KernelGDTRegister);
	LoadGDTRegister(&KernelGDTRegister);
	KernelCodeSegValue = GetCodeSegValue();
	KernelDataSegValue = GetDataSegValue();

	//	IDT, ISRs, IRQs
	InitIDTRegister(KernelIDT, KERNEL_IDT_ENTRIES_COUNT, &KernelIDTRegister);
	LoadIDTRegister(&KernelIDTRegister);
	InitSoftwareIntHandlers(KernelIDT, KernelCodeSegValue);
	RemapPIC(0x20, 0x28);
	MaskPIC();
	InitHardwareIntHandlers(KernelIDT, KernelCodeSegValue);
	ENABLE_HARDWARE_INTERRUPTS();

	//	PIT
	InitPIT(PIT_SOFTWARE_FREQUENCY);

	for (SIZE_T i = 0; i < 5; i++) {
		Sleep(1000);
		PutString("One second passed\r\n", BIOS_COLOR_WHITE);
	}

	PutString("Done\r\n", BIOS_COLOR_YELLOW);
	STOP();
}

GDT_REGISTER KernelGDTRegister;
GDT_ENTRY KernelGDT[KERNEL_GDT_ENTRIES_COUNT] = {};
IDT_REGISTER KernelIDTRegister;
IDT_ENTRY KernelIDT[KERNEL_IDT_ENTRIES_COUNT] = {};
UINT16 KernelCodeSegValue;
UINT16 KernelDataSegValue;