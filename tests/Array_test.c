/*
Copyright (c) 2011-2012 Hiroshi Tsubokawa
See LICENSE and README
*/

#include "Test.h"
#include "Array.h"
#include <stdio.h>

int main(int argc, const char **argv)
{
	{
		struct Array *a;
		int i = 0;
		int *pa = NULL;

		a = ArrNew(sizeof(int));
		TEST(a->data==NULL);
		TEST(a->nelems==0);
		TEST(a->nallocs==0);

		pa = (int *) ArrGrow(a, 3); 

		TEST(a->nelems==0);
		TEST(a->nallocs==3);
		TEST(pa!=NULL);

		i = 123;
		pa = (int *) ArrPush(a, &i);
		TEST(a->nelems==1);
		TEST(a->nallocs==3);
		TEST(pa!=NULL);
		TEST(pa[0]==123);

		i = -329;
		pa = (int *) ArrPush(a, &i);
		TEST(a->nelems==2);
		TEST(a->nallocs==3);
		TEST(pa!=NULL);
		TEST(pa[0]==123);
		TEST(pa[1]==-329);

		i = 1120;
		pa = (int *) ArrPush(a, &i);
		TEST(a->nelems==3);
		TEST(a->nallocs==3);
		TEST(pa!=NULL);
		TEST(pa[0]==123);
		TEST(pa[1]==-329);
		TEST(pa[2]==1120);

		ArrFree(a);
	}
	{
		struct Array *a;
		int i = 0;
		int *pa = NULL;

		a = ArrNew(sizeof(int));
		TEST(a->data==NULL);
		TEST(a->nelems==0);
		TEST(a->nallocs==0);

		i = 123;
		pa = (int *) ArrPush(a, &i);
		TEST(a->nelems==1);
		TEST(a->nelems<=a->nallocs);
		TEST(pa!=NULL);
		TEST(pa[0]==123);

		i = -329;
		pa = (int *) ArrPush(a, &i);
		TEST(a->nelems==2);
		TEST(a->nelems<=a->nallocs);
		TEST(pa!=NULL);
		TEST(pa[0]==123);
		TEST(pa[1]==-329);

		i = 1120;
		pa = (int *) ArrPush(a, &i);
		TEST(a->nelems==3);
		TEST(a->nelems<=a->nallocs);
		TEST(pa!=NULL);
		TEST(pa[0]==123);
		TEST(pa[1]==-329);
		TEST(pa[2]==1120);

		ArrFree(a);
	}
	printf("%s: %d/%d/%d: (FAIL/PASS/TOTAL)\n", __FILE__,
		TestGetFailCount(), TestGetPassCount(), TestGetTotalCount());

	return 0;
}

