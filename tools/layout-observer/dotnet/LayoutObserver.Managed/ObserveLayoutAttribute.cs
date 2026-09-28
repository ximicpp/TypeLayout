namespace LayoutObserver.Managed;

/// <summary>Registers a closed value type for a statically generated layout probe.</summary>
[AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
public sealed class ObserveLayoutAttribute(Type type, string typeId) : Attribute
{
    public Type Type { get; } = type;
    public string ObservationId { get; } = typeId;
}
