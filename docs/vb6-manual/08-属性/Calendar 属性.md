# Calendar 属性

# Calendar 属性

           

返回或设置一个值，指出工程中所使用日历的类型。

可以为 **Calendar**使用下列设置之一:

|                |        |                            |
|----------------|--------|----------------------------|
| **设置**       | **值** | **描述**                   |
| **vbCalGreg**  | 0      | 使用Gregorian 日历(缺省)。 |
| **vbCalHijri** | 1      | 使用Hijri 日历。           |

  

**说明**

可以程序化地只设置**Calendar** 属性。例如，要使用Hijri 日历，使用：

    Calendar = vbCalHijri
